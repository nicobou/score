#include "ConstraintModel.hpp"

#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Event/EventModel.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include <tools/utilsCPP11.hpp>
#include "ProcessInterface/ProcessFactoryInterface.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <API/Headers/Editor/TimeBox.h>

#include <QDebug>
#include <QApplication>





ConstraintModel::ConstraintModel(int id,
								 id_type<AbstractConstraintViewModel> fullViewId,
								 QObject* parent):
	IdentifiedObject{id, "ConstraintModel", parent},
	m_timeBox{new OSSIA::TimeBox},
	m_fullViewModel{
		new TemporalConstraintViewModel{fullViewId, this, this}
		}
{
	setupConstraintViewModel(m_fullViewModel);
	metadata.setName(QString("Constraint.%1").arg((SettableIdentifier::identifier_type) this->id()));
}

ConstraintModel::ConstraintModel(int id, id_type<AbstractConstraintViewModel> fullViewId, double yPos, QObject *parent):
	ConstraintModel{id, fullViewId, parent}
{
	setHeightPercentage(yPos);
}

ConstraintModel::~ConstraintModel()
{
	delete m_timeBox;
}


void ConstraintModel::setupConstraintViewModel(AbstractConstraintViewModel* viewmodel)
{
	connect(this,		&ConstraintModel::boxRemoved,
			viewmodel,	&AbstractConstraintViewModel::on_boxRemoved);

	connect(viewmodel, &QObject::destroyed,
			this,	   &ConstraintModel::on_destroyedViewModel);

	m_constraintViewModels.push_back(viewmodel);
}

void ConstraintModel::on_destroyedViewModel(QObject* obj)
{
	int index = m_constraintViewModels.indexOf(static_cast<AbstractConstraintViewModel*>(obj));
	if(index != -1)
		m_constraintViewModels.remove(index);
}

//// Complex commands
void ConstraintModel::createProcess(QString processName, int processId)
{
	auto model = ProcessList::getFactory(processName)->makeModel(processId, this);
	addProcess(model);
}



void ConstraintModel::addProcess(ProcessSharedModelInterface* model)
{
	m_processes.push_back(model);
	emit processCreated(model->processName(),
						(SettableIdentifier::identifier_type) model->id());
}

void ConstraintModel::removeProcess(int processId)
{
	auto proc = process(processId);
	vec_erase_remove_if(m_processes,
						[&processId] (ProcessSharedModelInterface* model)
						{ return model->id() == processId; });

	emit processRemoved(processId);
	delete proc;
}


void ConstraintModel::createBox(int boxId)
{
	auto box = new BoxModel{boxId, this};
	addBox(box);
}

void ConstraintModel::addBox(BoxModel* box)
{
	connect(this,	&ConstraintModel::processRemoved,
			box,	&BoxModel::on_deleteSharedProcessModel);

	m_boxes.push_back(box);
	emit boxCreated((SettableIdentifier::identifier_type) box->id());
}


void ConstraintModel::removeBox(int boxId)
{
	auto b = box(boxId);
	vec_erase_remove_if(m_boxes,
						[&boxId] (BoxModel* model)
						{ return model->id() == boxId; });

	emit boxRemoved(boxId);
	delete b;
}

id_type<EventModel> ConstraintModel::startEvent() const
{
	return m_startEvent;
}

id_type<EventModel> ConstraintModel::endEvent() const
{
	return m_endEvent;
}

void ConstraintModel::setStartEvent(id_type<EventModel> e)
{
	m_startEvent = e;
}

void ConstraintModel::setEndEvent(id_type<EventModel> e)
{
	m_endEvent = e;
}

BoxModel*ConstraintModel::box(int contentId) const
{
	return findById(m_boxes, contentId);
}

ProcessSharedModelInterface* ConstraintModel::process(int processId) const
{
	return findById(m_processes, processId);
}




int ConstraintModel::startDate() const
{
	return m_x;
}

void ConstraintModel::setStartDate(int start)
{
	m_x = start;
}

void ConstraintModel::translate(int deltaTime)
{
	m_x += deltaTime;
}

// Simple getters and setters

double ConstraintModel::heightPercentage() const
{
	return m_heightPercentage;
}

int ConstraintModel::minDuration() const
{
	return m_minDuration;
}

int ConstraintModel::maxDuration() const
{
	return m_maxDuration;
}

int ConstraintModel::defaultDuration() const
{
	return m_defaultDuration;
}

void ConstraintModel::setDefaultDuration(int width)
{
	if (m_defaultDuration != width)
	{
		setMinDuration(minDuration() + (width - defaultDuration()));
		setMaxDuration(maxDuration() + (width - defaultDuration()));

		m_defaultDuration = width;
		emit defaultDurationChanged(width);
	}
}

void ConstraintModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
		m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
	}
}

void ConstraintModel::setMinDuration(int arg)
{
	if (m_minDuration != arg) {
		m_minDuration = arg;
		emit minDurationChanged(arg);
	}
}

void ConstraintModel::setMaxDuration(int arg)
{
	if (m_maxDuration != arg) {
		m_maxDuration = arg;
		emit maxDurationChanged(arg);
	}
}

