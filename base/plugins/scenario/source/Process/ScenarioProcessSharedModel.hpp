#pragma once
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ScenarioProcessSharedModelSerialization.hpp"
#include <tools/SettableIdentifierAlternative.hpp>

namespace OSSIA
{
	class Scenario;
}
class TimeNodeModel;
class ConstraintModel;
class EventModel;
using IdentifiedEventModel = id_mixin<EventModel>;
class AbstractScenarioProcessViewModel;
class AbstractConstraintViewModel;

/**
 * @brief The ScenarioProcessSharedModel class
 *
 * Creation methods return tuples with the identifiers of the objects in their temporal order.
 * (first to last)
 */
class ScenarioProcessSharedModel : public ProcessSharedModelInterface
{
		Q_OBJECT

		friend void Visitor<Reader<DataStream>>::readFrom<ScenarioProcessSharedModel>(const ScenarioProcessSharedModel&);
		friend void Visitor<Writer<DataStream>>::writeTo<ScenarioProcessSharedModel>(ScenarioProcessSharedModel&);
		friend class ScenarioProcessFactory;

	public:
		using view_model_type = AbstractScenarioProcessViewModel;

		ScenarioProcessSharedModel(int id, QObject* parent);

		virtual ~ScenarioProcessSharedModel();
		virtual ProcessViewModelInterface* makeViewModel(int viewModelId,
														 QObject* parent) override;

		virtual QString processName() const override
		{
			return "Scenario";
		}

		// High-level operations (maintaining consistency)
		/**
		 * @brief createConstraintBetweenEvents
		 *
		 * Creates a new constraint between two existing events
		 */
		void createConstraintBetweenEvents(id_type<EventModel> startEventId,
										   id_type<EventModel> endEventId,
										   int newConstraintModelId,
										   id_type<AbstractConstraintViewModel> newConstraintFullViewId);

		/**
		 * @brief createConstraintAndEndEventFromEvent Base building block of a scenario.
		 *
		 * Given a starting event and a duration, creates an constraint and an event where
		 * the constraint is linked to both events.
		 */
		void createConstraintAndEndEventFromEvent(id_type<EventModel> startEventId,
												  int duration,
												  double heightPos,
												  int newConstraintId,
												  id_type<AbstractConstraintViewModel> newConstraintFullViewId,
												  id_type<EventModel> newEventId,
												  int newTimeNodeId);


		void moveEventAndConstraint(id_type<EventModel> eventId, int time, double heightPosition);
		void moveConstraint(int constraintId, int deltaX, double heightPosition);
		void moveNextElements(id_type<EventModel> firstEventMovedId, int deltaTime, QVector<id_type<EventModel> >& movedEvent);


		// Low-level operations (the caller has the responsibility to maintain the consistency of the scenario)
		void addConstraint(ConstraintModel* constraint);
		void addEvent(IdentifiedEventModel* event);

		void removeConstraint(int constraintId);
		void removeEvent(id_type<EventModel> eventId);
		void removeEventFromTimeNode(id_type<EventModel> eventId);
		void undo_createConstraintAndEndEventFromEvent(int constraintId);
		void undo_createConstraintBetweenEvent(int constraintId);


		// Accessors
		ConstraintModel* constraint(int constraintId) const;
		IdentifiedEventModel* event(id_type<EventModel> eventId) const;
		TimeNodeModel* timeNode(int timeNodeId) const;

		IdentifiedEventModel* startEvent() const;
		IdentifiedEventModel* endEvent() const;

		// Here, a copy is returned because it might be possible
		// to call a method on the scenario (e.g. removeConstraint) that changes the vector
		// while iterating, which would invalidate the iterators
		// and lead to undefined behaviour
		std::vector<ConstraintModel*> constraints() const
		{ return m_constraints; }
		std::vector<IdentifiedEventModel*> events() const;
		std::vector<TimeNodeModel*> timeNodes() const
		{ return m_timeNodes; }

	signals:
		void eventCreated(id_type<EventModel> eventId);
		void constraintCreated(int constraintId);
		void timeNodeCreated(int timeNodeId);
		void eventRemoved(id_type<EventModel> eventId);
		void constraintRemoved(int constraintId);
		void eventMoved(id_type<EventModel> eventId);
		void constraintMoved(int constraintId);

		void locked();
		void unlocked();

	public slots:
		void lock()
		{ emit locked(); }
		void unlock()
		{ emit unlocked(); }

		void on_viewModelDestroyed(QObject*);

	protected:
		template<typename Impl>
		ScenarioProcessSharedModel(Deserializer<Impl>& vis, QObject* parent):
			ProcessSharedModelInterface{vis, parent}
		{
			vis.writeTo(*this);
		}

		virtual ProcessViewModelInterface* makeViewModel(SerializationIdentifier identifier,
														 void* data,
														 QObject* parent) override;

		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const override;

		// To prevent warnings in Clang
		virtual bool event(QEvent* e) override
		{ return QObject::event(e); }
	private:
		void makeViewModel_impl(view_model_type*);

		OSSIA::Scenario* m_scenario;

		std::vector<ConstraintModel*> m_constraints;
		std::vector<IdentifiedEventModel*> m_events;
		std::vector<TimeNodeModel*> m_timeNodes;

		id_type<EventModel> m_startEventId{};
		id_type<EventModel> m_endEventId{};
};
