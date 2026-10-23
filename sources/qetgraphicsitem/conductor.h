/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.
	
	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	
	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CONDUCTOR_H
#define CONDUCTOR_H
#include "../autoNum/assignvariables.h"
#include "../conductorproperties.h"

#include <QGraphicsPathItem>
#include <QPointer>
#include <QSet>
#include <QUuid>

class ConductorProfile;
class DiagramContext;
class ConductorSegmentProfile;
class Diagram;
class projectDataBase;
class Terminal;
class ConductorSegment;
class ConductorTextItem;
class Element;
class QETDiagramEditor;
class NumerotationContext;
class QetGraphicsHandlerItem;

typedef QPair<QPointF, Qt::Corner> ConductorBend;
typedef QHash<Qt::Corner, ConductorProfile> ConductorProfilesGroup;
/**
	This class represents a conductor, i.e. a wire between two element
	terminals.
*/
class Conductor : public QGraphicsObject
{
	Q_OBJECT

	Q_PROPERTY(QPointF pos READ pos WRITE setPos)
	Q_PROPERTY(int animPath READ fakePath WRITE updatePathAnimate)
	Q_PROPERTY(ConductorProperties properties READ properties WRITE setProperties)
	Q_PROPERTY(autonum::sequentialNumbers sequenceNum READ sequenceNum WRITE setSequenceNum)
	// True: left out of the automatic numbering - no routine writes its
	// text, and its number is not counted (see isFreezeLabel()).
	Q_PROPERTY(bool freezeLabel READ isFreezeLabel WRITE setFreezeLabel NOTIFY freezeLabelChanged)

	signals:
		void propertiesChange();
		void freezeLabelChanged();
	
	public:
		Conductor(Terminal *, Terminal *);
		~Conductor() override;

		bool isValid() const;
	
	private:
		Conductor(const Conductor &);

	public:
		enum { Type = UserType + 1001 };
		enum Highlight { None, Normal, Alert };

		Terminal *terminal1;
		Terminal *terminal2;
	
	public:
		/**
			@brief type
			Enable the use of qgraphicsitem_cast
			to safely cast a QGraphicsItem into a conductor.
			@return the QGraphicsItem type
		*/
		int type() const override { return Type; }
		Diagram *diagram() const;
		ConductorTextItem *textItem() const;
		QUuid uuid() const {return m_uuid;}
		void newUuid();	//create new uuid for this conductor
		/**
		@brief persistUuid
		Say that this conductor identity has to reach the file.

		fromXml() clears m_persist_uuid for a project whose conductors carry
		no uuid, so that a legacy file is not rewritten with a fresh random
		one on every load-and-resave (#754). That is right for a conductor
		nobody names -- and wrong for one something else records by uuid:
		dropped from the file, the conductor comes back with a different
		identity and whatever named it is orphaned. Whoever writes the
		reference says so here.
		*/
		void persistUuid() {m_persist_uuid = true;}
		void setUuid(const QUuid &uuid);	//saved from now on
		bool replaceTerminal(Terminal *from, Terminal *to);
		void dockAt(Terminal *end1, Terminal *end2);
		void updatePath(const QRectF & = QRectF());

		//This method do nothing, it's only made to be used with Q_PROPERTY
		//It's used to anim the path when is change
		void updatePathAnimate(const int = 1) {updatePath();}
		int fakePath() {return 1;}

		void paint(
				QPainter *,
				const QStyleOptionGraphicsItem *,
				QWidget *) override;
		QRectF boundingRect() const override;
		QPainterPath shape() const override;
		virtual QPainterPath nearShape() const;
		qreal length() const;
		ConductorSegment *middleSegment();
		QPointF posForText(Qt::Orientations &flag, qreal *run_length = nullptr);
		void refreshText();
		void setPath(const QPainterPath &path);
		QPainterPath path() const;

	private:
			/// A run the name may be written along: its middle, its
			/// direction and its length
		struct TextRun
		{
			QPointF middle;
			Qt::Orientations along;
			qreal length;
		};
		QVector<TextRun> textRuns();
		bool placeTextOn(const TextRun &run);
		QList<Conductor *> pointPartners();
		QList<Conductor *> pointGroup();
		bool nameHoldsOnOwnPiece();
		qreal longestSharedPiece();
		bool sharedLineHoldsName();
		bool placeNameOnSharedLine(const QList<Conductor *> &group);

	public:
			/// A live move - a drag of components or of a segment - begins
			/// and ends: in between, the names of a point are not decided
			/// again at each step, only once at its end (T67, tenth
			/// adjustment). Nested calls count.
		static void beginLiveMove();
		static void endLiveMove();
		static bool valideXml (QDomElement &);
		bool fromXml (QDomElement &);
		QDomElement toXml (
				QDomDocument &,
				QHash<Terminal *,
				int> &,
				const QSet<QUuid> &shared_uuids = QSet<QUuid>()) const;
	private:
		bool pathFromXml(const QDomElement &);

	public:
		QVector <QPointF> handlerPoints() const;
		const QList<ConductorSegment *> segmentsList() const;
		bool moveSegment(int index, qreal dx, qreal dy);
		bool setPathPoints(const QList<QPointF> &scene_points);
		bool setArrangedPath(const QList<QPointF> &scene_points);
		QList<QPointF> automaticPathPoints() const;
		bool hasSavedPath() const;
			/// Whether the saved path of this conductor was placed by the
			/// application (WireArrangement) and not dragged by the user:
			/// it may then be moved again when a new conductor comes next
			/// to it
		bool isAutoArranged() const {return m_auto_arranged;}
		void setAutoArranged(bool arranged) {m_auto_arranged = arranged;}

		void setPropertyToPotential(
				const ConductorProperties &property,
				bool only_text = false);
		void setProperties(const ConductorProperties &property);
		ConductorProperties properties() const;
			/// This wire's own copy of its properties, whatever the store holds
		ConductorProperties ownProperties() const {return m_properties;}
			/// The order wires were made in this session: a project's are
			/// made in the order its file lists them
		quint64 creationRank() const {return m_creation_rank;}
			/// Records which cable core this conductor shows and writes the
			/// label that assignment generates (15W1:br) into its cable
			/// field. Deliberately not routed through
			/// setPropertyToPotential(): a core of a cable belongs to this
			/// conductor alone and must never spill over a pass-through
			/// terminal onto the neighbouring half conductor.
		void setCableReference(const QString &label,
							   const QUuid &cable_uuid,
							   int slot);

		void setProfile(const ConductorProfile &, Qt::Corner);
		ConductorProfile profile(Qt::Corner) const;
		void setProfiles(const ConductorProfilesGroup &);
		ConductorProfilesGroup profiles() const;
		void calculateTextItemPosition();
		void updateTextVisibility();
		virtual Highlight highlight() const;
		virtual void setHighlighted(Highlight);
		QSet<Conductor *> relatedPotentialConductors(
				const bool all_diagram = true,
				QList <Terminal *> *t_list=nullptr,
				const bool ignore_function_separation = false);
		QSet<Conductor *> functionSeparatedConductors(
				const bool all_diagram = true);
		QETDiagramEditor* diagramEditor() const;
		void editProperty ();

		autonum::sequentialNumbers sequenceNum () const
		{return m_autoNum_seq;}
		autonum::sequentialNumbers& rSequenceNum()
		{return m_autoNum_seq;}
		void setSequenceNum(const autonum::sequentialNumbers& sn);

		QList<QPointF> junctions() const;
		QPainterPath paintedPath() const;

			/// The wire this one branches off (T86): its path is the path
			/// of the arm from the terminal they share to the point, and its
			/// own piece from there to its other terminal. nullptr for a
			/// wire of its own, and while the arm is off the folio
		Conductor *arm() const;
			/// Where on its arm this wire branches off, in scene coordinates
		QPointF armPoint() const {return m_arm_point;}
		bool canBranchOff(const Conductor *arm) const;
		bool setArm(Conductor *arm, const QPointF &scene_point);
		void clearArm();
			/// The wires that branch off this one
		QList<Conductor *> branches() const;
			/// The path of this conductor, in scene coordinates, from
			/// terminal1 to terminal2
		QList<QPointF> scenePathPoints() const;
			/// Where the own piece of a branch starts on its path, counted in
			/// points from terminal1; -1 for a wire of its own
		int ownPieceStart() const {return m_own_from;}
		static Terminal *sharedTerminal(const Conductor *a, const Conductor *b);
			/// The end of this wire nearer to @a scene_point along it: the
			/// terminal a branch off that point shares the wire from
		Terminal *nearerEndTo(const QPointF &scene_point) const;
		static QPointF nearestOnPath(const QList<QPointF> &points, const QPointF &point,
					     int *segment = nullptr);
		static void linkArms(const QList<Conductor *> &conductors);
			/// Something the wires hop over changed shape - the trunk of a
			/// bus bar (T87): every wire works its hops out again
		static void othersChangedShape();

		/**
			@brief While one of these lives, a conductor asked to place its
			name only notes it, and the names are placed once, each, when
			the last one goes: what reading a folio does (Diagram::fromXml()).
			Placed as each wire came in, the name of each conductor of a
			point was placed again every time another wire of the point
			changed - the arm and its branches all of them -, and a folio of
			17 branches took 18 s to show (23/10/2026)
		*/
		class TextPlacementDeferral
		{
			public:
				TextPlacementDeferral();
				~TextPlacementDeferral();
				TextPlacementDeferral(const TextPlacementDeferral &) = delete;
				TextPlacementDeferral &operator=(const TextPlacementDeferral &) = delete;
		};
			/// The own piece of a branch as the person left it, in scene
			/// coordinates, from the arm to its terminal; empty while it is the
			/// automatic one, straight out of the terminal (T86, 23/10/2026)
		QList<QPointF> ownShape() const {return m_own_points;}
		void setBranchShape(const QPointF &arm_point, const QList<QPointF> &own_points);
			/// The own piece of a branch, in scene coordinates, from the arm to
			/// its terminal, as it is drawn now
		QList<QPointF> ownPiecePoints() const;
			/// Whether the segment @a index of the path is of the own piece of
			/// this branch - the only ones a branch lets the mouse drag
		bool isOwnSegment(int index) const;

	private:
		void followArm();
		QList<QPointF> branchPathPoints(int *own_from = nullptr) const;
		QPainterPath ownPiecePath() const;
		QPainterPath ownPiecePaintedPath() const;
		static bool straightOnto(const QList<QPointF> &along, const QPointF &end,
					 Qet::Orientation orientation, QPointF *at, int *segment);
		static QList<QPointF> keptShape(QList<QPointF> points, const QPointF &at, const QPointF &end);
		void setUpConnectionForFormula(
				QString old_formula, QString new_formula);
		autonum::sequentialNumbers m_autoNum_seq;

	public:
		void setFreezeLabel(bool freeze);
			/**
				Whether the wire is left out of the automatic numbering: the
				text it has stays - its formula is not worked out - and the
				routines that number, name or count wires pass it by. The
				one the user types in its own panel still reaches it.
			*/
		bool isFreezeLabel() const {return m_freeze_label;}

	private:
		/**
			@brief Hear about the location of the component holding @a terminal.

			The stroke of a wire depends on where its two ends stand, and that
			is read at paint time - so something has to say when it moved. See
			elementInformationChanged() for why the listening is done from
			here rather than from the component.
		*/
		void watchLocationOf(Terminal *terminal);
		void elementInformationChanged(
				const DiagramContext &old_info,
				const DiagramContext &new_info);
	
	public slots:
		void displayedTextChanged();
	
	protected:
		void mouseDoubleClickEvent(
				QGraphicsSceneMouseEvent *event) override;
		void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent(
				QGraphicsSceneMouseEvent *event) override;
		void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
		void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
		QVariant itemChange(
				GraphicsItemChange, const QVariant &) override;
		bool sceneEventFilter(
				QGraphicsItem *watched, QEvent *event) override;

	private:
		void adjustHandlerPos();
		
		void handlerMousePressEvent(
				QetGraphicsHandlerItem *qghi,
				QGraphicsSceneMouseEvent *event);
		void handlerMouseMoveEvent(
				QetGraphicsHandlerItem *qghi,
				QGraphicsSceneMouseEvent *event);
		void handlerMouseReleaseEvent(
				QetGraphicsHandlerItem *qghi,
				QGraphicsSceneMouseEvent *event);
		void addHandler();
		void removeHandler();
		
		
		QVector<QetGraphicsHandlerItem *> m_handler_vector;
		int m_vector_index = -1;
		bool m_mouse_over;
			///Hop path cache for paintedPath(), valid while no conductor geometry changed
		mutable QPainterPath m_hops_path;
		mutable quint64 m_hops_generation = 0;
		mutable int m_hops_mode = -1;
			/// Functional properties
		ConductorProperties m_properties;
			/// Text input for non simple, non-singleline conductors
		ConductorTextItem *m_text_item;
			/// Segments composing the conductor
		ConductorSegment *segments;
			/// Attributes related to mouse interaction
		bool m_moving_segment;
		int moved_point;
		qreal m_previous_z_value;
			/// The drag of a segment by its handle: the path when it started,
			/// the segment, and where its middle was (T68)
		QList<QPointF> m_drag_points;
		int m_drag_segment = -1;
		QPointF m_drag_middle;
		QPointF before_mov_text_pos_;
			/// The conductors of the potential that run along the dragged
			/// segment, dragged with it (T67, tenth adjustment): each with
			/// its path when the drag started and its segment in line
		struct DragPartner
		{
			QPointer<Conductor> conductor;
			QList<QPointF> points;
			int segment = -1;
		};
		QList<DragPartner> m_drag_partners;
			/// The drag of a segment is a live move (beginLiveMove())
		bool m_live_drag = false;
			/// The name of this conductor is shown once for its point, on
			/// the line the point shares, by another conductor of it - none
			/// of its own pieces holds it (T67, tenth adjustment)
		bool m_name_in_point = false;
			/// This conductor shows the name of its point, on the line the
			/// point shares
		bool m_shows_point_name = false;
			/// The conductors this one shared a line with the last time its
			/// name was placed: their names are placed again with it
		QList<QPointer<Conductor>> m_point_group;
			/// See beginLiveMove(): how many are running, and the conductors
			/// whose names were placed during them
		static int s_live_moves;
		static QList<QPointer<Conductor>> s_live_touched;
			/// Whether the conductor was manually modified by users
		bool modified_path;
			/// Whether the current profile should be saved as soon as possible
		bool has_to_save_profile;
			/// conductor profile: "photography" of what the conductor is supposed to look
			/// like - there is one profile per kind of traject
		ConductorProfilesGroup conductor_profiles;
			/// Define whether and how the conductor should be highlighted
		Highlight must_highlight_;
		bool m_valid;
		bool m_freeze_label = false;
			/// See isAutoArranged()
		bool m_auto_arranged = false;
			/// See arm(): the arm, kept while it is off the folio - deleted,
			/// and back with an undo -, the point on it, the wires that
			/// branch off this one, and the arm read from the file before the
			/// folio has all its wires (linkArms())
		QPointer<Conductor> m_arm;
		QPointF m_arm_point;
		QList<QPointer<Conductor>> m_branches;
		QUuid m_pending_arm;
		int m_own_from = -1;
			/// See ownShape()
		QList<QPointF> m_own_points;
		QUuid m_uuid;
			/// false when m_uuid was synthesized by fromXml() because the
			/// file had none -- toXml() must not persist that value, or
			/// every reload mints and saves a new random one (see #754).
		bool m_persist_uuid = true;
		quint64 m_creation_rank = 0;
		void uuidChanged(const QUuid &old_uuid);
		projectDataBase *placedStore() const;
		void storeProperties();

			/// QPen et QBrush objects used to draw conductors
		static QPen conductor_pen;
		static QBrush conductor_brush;
		static bool pen_and_brush_initialized;
		QPainterPath m_path;
	
	private:
		void segmentsToPath();
		void saveProfile(bool = true);
		void generateConductorPath(const QPointF &, Qet::Orientation, const QPointF &, Qet::Orientation);
		static QList<QPointF> automaticPath(const QPointF &, Qet::Orientation, const QPointF &, Qet::Orientation);
		bool applyPathPoints(const QList<QPointF> &scene_points, bool by_hand);
		void updateConductorPath(const QPointF &, Qet::Orientation, const QPointF &, Qet::Orientation);
		uint segmentsCount(QET::ConductorSegmentType = QET::Both) const;
		QList<QPointF> segmentsToPoints() const;
		QList<ConductorBend> bends() const;
		bool bendMakesJunction(const Conductor *, const QPointF &, Qt::Corner) const;

		void pointsToSegments(const QList<QPointF>&);
		Qt::Corner currentPathType() const;
		void deleteSegments();
		static int getCoeff(const qreal &, const qreal &);
		static int getSign(const qreal &);
		QHash<ConductorSegmentProfile *, qreal> shareOffsetBetweenSegments(const qreal &offset, const QList<ConductorSegmentProfile *> &, const qreal & = 0.01) const;
		static QPointF extendTerminal(const QPointF &, Qet::Orientation, qreal = 10);
		static Qt::Corner movementType(const QPointF &, const QPointF &);
		static QPointF movePointIntoPolygon(const QPointF &, const QPainterPath &);
};

Conductor * longestConductorInPotential (Conductor *conductor, bool all_diagram = false);
QList <Conductor *> relatedConductors (const Conductor *conductor);


//return true if @a is between or at @b and @c.
template <typename T>
bool isBetween (const T a, const T b, const T c) {
	return (b <= c)? (a >= b && a <= c) : (a <= b && a >= c);
}

#endif
