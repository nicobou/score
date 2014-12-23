#pragma once
#include <tools/IdentifiedObject.hpp>

#include <QColor>
#include <vector>

namespace OSSIA
{
	class TimeBox;
}

class ProcessSharedModelInterface;
class ConstraintViewModelInterface;

class BoxModel;
class EventModel;
class TimeBox;

/**
 * @brief The ConstraintModel class
 *
 * Contains at least 1 BoxModel (else nothing can be displayed)
 */
// TODO put some of this stuff in the corresponding view models.
class ConstraintModel : public IdentifiedObject
{
	Q_OBJECT
		Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
		Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)
		Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
		Q_PROPERTY(double heightPercentage READ heightPercentage WRITE setHeightPercentage NOTIFY heightPercentageChanged)

	public:


		friend QDataStream& operator << (QDataStream&, const ConstraintModel&);
		friend QDataStream& operator >> (QDataStream&, ConstraintModel&);

		ConstraintModel(int id, QObject* parent);
		ConstraintModel(int id, double yPos, QObject* parent);
		ConstraintModel(QDataStream&, QObject* parent);
		virtual ~ConstraintModel() = default;

		// Factories for the view models.
		template<typename ViewModelType, typename Arg> // Arg might be an id or a datastream
		ViewModelType* makeViewModel(Arg&& arg, QObject* parent)
		{
			auto viewmodel =  new ViewModelType{arg, this, parent};
			makeViewModel_impl(viewmodel);
			return viewmodel;
		}

		void makeViewModel_impl(ConstraintViewModelInterface* viewmodel);

		// Sub-element creation
		int createProcess(QString processName, int processId);
		int createProcess(QDataStream& data);
		void removeProcess(int processId);

		void createBox(int boxId);
		void createBox(QDataStream& s);
		void removeBox(int viewId);
		void duplicateBox(int viewId);

		int startEvent();
		int endEvent();
		void setStartEvent(int eventId); // Use ScenarioKey
		void setEndEvent(int eventId); // Use ScenarioKey

		BoxModel* box(int contentId);
		ProcessSharedModelInterface* process(int processId);

		QString name() const;
		QString comment() const;
		QColor color() const;


		OSSIA::TimeBox* apiObject()
		{ return m_timeBox;}

		// Copies are done because there might be a loop
		// that might change the vector, and invalidate the
		// iterators, leading to a crash quite difficult to debug.
		std::vector<BoxModel*> boxes() const
		{ return m_boxes; }
		std::vector<ProcessSharedModelInterface*> processes() const
		{ return m_processes; }

		double heightPercentage() const;
		int startDate() const;
		void setStartDate(int start);
		void translate(int deltaTime);

		int width() const;
		void setWidth(int width);

		int height() const;
		void setHeight(int height);

	public slots:
		void setName(QString arg);
		void setComment(QString arg);
		void setColor(QColor arg);
		void setHeightPercentage(double arg);

	signals:
		void processCreated(QString processName, int processId);
		void processRemoved(int processId);

		void boxCreated(int boxId);
		void boxRemoved(int boxId);

		void nameChanged(QString arg);
		void commentChanged(QString arg);
		void colorChanged(QColor arg);
		void heightPercentageChanged(double arg);

	private:
		// TODO maybe put this in public, and let the command call ProcessList and pass a pointer to createProcess (which becomes addProcess)
		int createProcess_impl(ProcessSharedModelInterface*);
		void createBox_impl(BoxModel*);

		OSSIA::TimeBox* m_timeBox{}; // Manages the duration

		std::vector<BoxModel*> m_boxes; // No content -> Phantom ?
		std::vector<ProcessSharedModelInterface*> m_processes;

		QString m_name{"Constraint."};
		QString m_comment;
		QColor m_color; // Maybe in ContentModel ?
		double m_heightPercentage{0.5}; // Relative y position of the top-left corner. Should maybe be in Scenario ?

		int m_startEvent{};
		int m_endEvent{};

		// ___ TEMPORARY ___
		int m_width{200};
		int m_height{200};
		int m_x{};

};
