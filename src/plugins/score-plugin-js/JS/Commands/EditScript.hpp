#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Script/ScriptEditor.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/Commands/JSCommandFactory.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
namespace JS
{
struct SavedPort {
  QString name;
  Process::PortType type;
  QByteArray data;
};

class EditScript : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      EditScript,
      "Edit a script")
  public:
    EditScript(const JS::ProcessModel& model, QString newScript)
    : m_path{model}
    , m_newScript{std::move(newScript)}
    , m_oldScript{model.script()}
  {
    m_oldCables = Dataflow::saveCables(
          {const_cast<JS::ProcessModel*>(&model)},
          score::IDocument::documentContext(model));
    for(auto& port : model.inlets())
      m_oldInlets.emplace_back(SavedPort{port->customData(), port->type(), port->saveData()});
    for(auto& port : model.outlets())
      m_oldOutlets.emplace_back(SavedPort{port->customData(), port->type(), port->saveData()});
  }

private:
  void undo(const score::DocumentContext& ctx) const override
  {
    auto& cmt = m_path.find(ctx);
    // Remove all the cables that could have been added during
    // the creation
    Dataflow::removeCables(m_oldCables, ctx);

    // Set the old script
    cmt.setScript(m_oldScript);

    // We expect the inputs / outputs to revert back to the
    // exact same state
    SCORE_ASSERT(m_oldInlets.size() == cmt.inlets().size());
    SCORE_ASSERT(m_oldOutlets.size() == cmt.outlets().size());

    // So we can reload their data identically
    for(std::size_t i = 0; i < m_oldInlets.size(); i++)
    {
      cmt.inlets()[i]->loadData(m_oldInlets[i].data);
    }
    for(std::size_t i = 0; i < m_oldOutlets.size(); i++)
    {
      cmt.outlets()[i]->loadData(m_oldOutlets[i].data);
    }

    // Recreate the old cables
    Dataflow::restoreCables(m_oldCables, ctx);
  }

  static void restoreCables(
      Process::Inlet& new_p,
      Scenario::ScenarioDocumentModel& doc,
      const score::DocumentContext& ctx,
      const Dataflow::SerializedCables& cables)
  {
    for(auto& cable : new_p.cables())
    {
      SCORE_ASSERT(!cable.unsafePath().vec().empty());
      auto cable_id = cable.unsafePath().vec().back().id();
      auto it = ossia::find_if(
            cables,
            [cable_id] (auto& c){ return c.first.val() == cable_id; }
      );

      SCORE_ASSERT(it != cables.end());
      SCORE_ASSERT(doc.cables.find(it->first) == doc.cables.end());
      {
        auto c = new Process::Cable{it->first, it->second, &doc};
        doc.cables.add(c);
        c->source().find(ctx).addCable(*c);
      }
    }
  }
  static void restoreCables(
      Process::Outlet& new_p,
      Scenario::ScenarioDocumentModel& doc,
      const score::DocumentContext& ctx,
      const Dataflow::SerializedCables& cables)
  {
    for(auto& cable : new_p.cables())
    {
      SCORE_ASSERT(!cable.unsafePath().vec().empty());
      auto cable_id = cable.unsafePath().vec().back().id();
      auto it = ossia::find_if(
            cables,
            [cable_id] (auto& c){ return c.first.val() == cable_id; }
      );

      SCORE_ASSERT(it != cables.end());
      SCORE_ASSERT(doc.cables.find(it->first) == doc.cables.end());
      {
        auto c = new Process::Cable{it->first, it->second, &doc};
        doc.cables.add(c);
        c->sink().find(ctx).addCable(*c);
      }
    }
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    Dataflow::removeCables(m_oldCables, ctx);

    auto& cmt = m_path.find(ctx);
    cmt.setScript(m_newScript);

    // Try an optimistic matching. Type and name must match.

    Scenario::ScenarioDocumentModel& doc
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

    std::size_t min_inlets = std::min(m_oldInlets.size(), cmt.inlets().size());
    std::size_t min_outlets = std::min(m_oldOutlets.size(), cmt.outlets().size());
    for(std::size_t i = 0; i < min_inlets; i++)
    {
      auto new_p = cmt.inlets()[i];
      auto& old_p = m_oldInlets[i];

      if(new_p->type() == old_p.type && new_p->customData() == old_p.name)
      {
        new_p->loadData(old_p.data);
        restoreCables(*new_p, doc, ctx, m_oldCables);
      }
    }

    for(std::size_t i = 0; i < min_outlets; i++)
    {
      auto new_p = cmt.outlets()[i];
      auto& old_p = m_oldOutlets[i];

      if(new_p->type() == old_p.type && new_p->customData() == old_p.name)
      {
        new_p->loadData(old_p.data);
        restoreCables(*new_p, doc, ctx, m_oldCables);
      }
    }
  }

  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_newScript << m_oldScript << m_oldInlets << m_oldOutlets <<  m_oldCables;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_newScript >> m_oldScript >> m_oldInlets >> m_oldOutlets >> m_oldCables;
  }

  Path<JS::ProcessModel> m_path;
  QString m_newScript;
  QString m_oldScript;

  std::vector<SavedPort> m_oldInlets, m_oldOutlets;

  Dataflow::SerializedCables m_oldCables;
};
}

template<>
struct is_custom_serialized<JS::SavedPort> : std::true_type { };

template <>
struct TSerializer<DataStream, JS::SavedPort>
{
  static void readFrom(DataStream::Serializer& s, const JS::SavedPort& tv)
  {
    s.stream() << tv.name << tv.type << tv.data;
  }

  static void writeTo(DataStream::Deserializer& s, JS::SavedPort& tv)
  {
    s.stream() >> tv.name >> tv.type >> tv.data;
  }
};
namespace score
{
template<>
struct StaticPropertyCommand<JS::ProcessModel::p_script> : JS::EditScript
{
  using JS::EditScript::EditScript;

};
}

