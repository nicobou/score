// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "JSProcessModel.hpp"

#include "JS/JSProcessMetadata.hpp"

#include <JS/Qml/QmlObjects.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/Expression.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlComponent>
#include <QDebug>

#include <wobjectimpl.h>

#include <vector>
W_OBJECT_IMPL(JS::ProcessModel)
namespace JS
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const QString& data,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
{
  if (data.isEmpty())
  {
    setScript(
        R"_(import Score 1.0
Script {
  ValueInlet { id: in1 }
  ValueOutlet { id: out1 }
  FloatSlider { id: sl; min: 10; max: 100; }

  tick: function(oldtime, time, position, offset) {
          console.log(in1.value);
    out1.value = in1.value + sl.value * Math.random();
  }
  start: function() { console.log("I am called on start"); }
  stop: function() { console.log("I am called on stop"); }
  pause: function() { console.log("I am called on pause"); }
  resume: function() { console.log("I am called on resume"); }
})_");
  }
  else
  {
    setScript(data);
  }
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
}

void ProcessModel::setScript(const QString& script)
{
  m_watch.reset();
  /*
  if (m_dummyObject)
    m_dummyObject->deleteLater();
  m_dummyObject = nullptr;
  m_dummyComponent.reset();
  m_dummyComponent = std::make_unique<QQmlComponent>(&m_dummyEngine);
  */
  m_script = script;
  scriptChanged(script);
  auto trimmed = script.trimmed();

  QByteArray data = trimmed.toUtf8();

  auto path = score::locateFilePath(
      trimmed, score::IDocument::documentContext(*this));

  if (QFileInfo{path}.exists())
  {
    /* Disabling the watch feature for now :
     * it does not fix the cables, etc.
    m_watch = std::make_unique<QFileSystemWatcher>(QStringList{trimmed});
    connect(
        m_watch.get(),
        &QFileSystemWatcher::fileChanged,
        this,
        [=](const QString& path) {
          // Note:
          // https://stackoverflow.com/questions/18300376/qt-qfilesystemwatcher-signal-filechanged-gets-emited-only-once
          QTimer::singleShot(20, this, [this, path] {
            m_watch->addPath(path);
            QFile f(path);
            if (f.open(QIODevice::ReadOnly))
            {
              setQmlData(path.toUtf8(), true);
              m_watch->addPath(path);
            }
          });
        });

    */
    setQmlData(path.toUtf8(), true);
  }
  else
  {
    setQmlData(data, false);
  }
}

void ProcessModel::setQmlData(const QByteArray& data, bool isFile)
{
  if (!isFile && !data.startsWith("import"))
    return;

  m_isFile = isFile;
  m_qmlData = data;

  auto script = m_cache.get(*this, data, isFile);
  if(!script)
    return;

  auto old_inlets = score::clearAndDeleteLater(m_inlets);
  auto old_outlets = score::clearAndDeleteLater(m_outlets);

  SCORE_ASSERT(m_inlets.size() == 0);
  SCORE_ASSERT(m_outlets.size() == 0);

  {
    auto cld_inlet = script->findChildren<Inlet*>();
    int i = 0;
    for (auto n : cld_inlet)
    {
      auto port = n->make(Id<Process::Port>(i++), this);
      port->setCustomData(n->objectName());
      if (auto addr = State::parseAddressAccessor(n->address()))
        port->setAddress(std::move(*addr));
      m_inlets.push_back(port);
    }
  }

  {
    auto cld_outlet = script->findChildren<Outlet*>();
    int i = 0;
    for (auto n : cld_outlet)
    {
      auto port = n->make(Id<Process::Port>(i++), this);
      port->setCustomData(n->objectName());
      if (auto addr = State::parseAddressAccessor(n->address()))
        port->setAddress(std::move(*addr));
      m_outlets.push_back(port);
    }
  }
  scriptOk();

  qmlDataChanged(data);
  inletsChanged();
  outletsChanged();
}

Script* ProcessModel::currentObject() const noexcept
{
  return m_cache.tryGet(m_qmlData, m_isFile);

}

ComponentCache::ComponentCache() { }
ComponentCache::~ComponentCache() { }

Script* ComponentCache::tryGet(const QByteArray& str, bool isFile) const noexcept
{
  QByteArray content;
  if (isFile)
  {
    QFile f{str};
    f.open(QIODevice::ReadOnly);
    content = f.readAll();
  }
  else
  {
    content = str;
  }

  auto it = ossia::find_if(m_map, [&] (const auto& k) { return k.key == content; });
  if(it != m_map.end())
  {
    return it->object.get();
  }
  else
  {
    return nullptr;
  }
}

Script* ComponentCache::get(ProcessModel& process, const QByteArray& str, bool isFile) noexcept
{
  QByteArray content;
  if (isFile)
  {
    QFile f{str};
    f.open(QIODevice::ReadOnly);
    content = f.readAll();
  }
  else
  {
    content = str;
  }

  auto it = ossia::find_if(m_map, [&] (const auto& k) { return k.key == content; });
  if(it != m_map.end())
  {
    return it->object.get();
  }
  else
  {
    auto comp = std::make_unique<QQmlComponent>(&process.engine());
    if(!isFile)
    {
      comp->setData(str, QUrl());
    }
    else
    {
      comp->loadUrl(QUrl::fromLocalFile(str));
    }

    const auto& errs = comp->errors();
    if (!errs.empty())
    {
      const auto& err = errs.first();
      qDebug() << err.line() << err.toString();
      process.errorMessage(err.line(), err.toString());
      return nullptr;
    }

    auto obj = comp->create();
    auto script = qobject_cast<JS::Script*>(obj);
    if(script)
    {
      if(m_map.size() > 5)
        m_map.erase(m_map.begin());

      m_map.emplace_back(Cache{str, std::move(comp), std::unique_ptr<JS::Script>(script)});
      return script;
    }
    else
    {
      process.errorMessage(0, "The component must be of type Script");
      if(obj)
      {
        delete obj;
      }
      return nullptr;
    }
  }
}

}
