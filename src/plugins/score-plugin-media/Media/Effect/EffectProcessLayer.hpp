#pragma once

#include <Dataflow/UI/PortItem.hpp>
#include <Media/Commands/InsertEffect.hpp>
#include <Media/Effect/EffectProcessMetadata.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QDrag>
#include <QFileInfo>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QJsonDocument>
#include <QMetaProperty>
#include <QPainter>
#include <QWindow>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectLayer.hpp>

namespace score
{
namespace mime
{
inline constexpr auto effect()
{
  return "application/x-score-fxdata";
}
}
}
namespace Media::Effect
{
class EffectTitleItem;
struct ControlUi
{
  Process::ControlInlet* inlet;
  score::RectItem* rect;
};

struct EffectUi
{
  EffectUi(const Process::ProcessModel& fx, score::RectItem* rt)
      : effect{fx}, root_item{rt}
  {
  }
  ~EffectUi()
  {
    for (auto& con : cons)
      QObject::disconnect(con);
  }
  const Process::ProcessModel& effect;
  score::RectItem* root_item{};
  score::ResizeableItem* fx_item{};
  EffectTitleItem* title{};
  std::vector<Dataflow::PortItem*> inlets;
  std::vector<Dataflow::PortItem*> outlets;

  std::vector<QMetaObject::Connection> cons;
};

class EffectTitleItem final : public QObject, public QGraphicsItem
{
  W_OBJECT(EffectTitleItem)
public:
    QRectF m_rect;
  void setRect(QRectF rect)
  {
    prepareGeometryChange();
    m_rect = rect;
    if (m_ui)
      m_ui->setPos({m_rect.width() - m_ui->boundingRect().width() - 7., 2.});
  }
  EffectTitleItem(
      Process::ProcessModel& effect,
      const Effect::ProcessModel& object,
      const Process::LayerContext& ctx,
      QObject* parent,
      QGraphicsItem* parentItem,
      EffectUi& ui)
      : QObject{parent},
        QGraphicsItem{parentItem},
        m_effect{effect}
  {
    const auto& doc = ctx.context;
    const auto& pixmaps = Process::Pixmaps::instance();
    const auto& skin = Process::Style::instance();

    if (m_ui = Process::makeExternalUIButton(effect, ctx.context, this, this))
      m_ui->setPos({m_rect.width() - m_ui->boundingRect().width() - 7., 2.});

    m_label = new score::SimpleTextItem{score::Skin::instance().Base4.main, this};
    m_label->setText(effect.prettyName());
    m_label->setFont(skin.skin.Bold10Pt);
    m_label->setPos({10., 0.});

    connect(this, &EffectTitleItem::clicked, this, [&] {
      doc.focusDispatcher.focus(&ctx.presenter);
      score::SelectionDispatcher{doc.selectionStack}.setAndCommit({&effect});
    });
  }
  void setLabelPos(QPointF p)
  {
    m_label->setPos(p);
  }
  QRectF boundingRect() const override { return m_rect; }
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override
  {
    auto& skin = score::Skin::instance();
    const auto& pen = skin.NoPen;
    const auto& brush = skin.Emphasis5.main.brush;
    const auto& highlightbrush = skin.Emphasis5.lighter.brush;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(pen);
    painter->setBrush(m_highlight ? highlightbrush : brush);
    painter->drawRoundedRect(m_rect, 5, 5);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  void setHighlight(bool b) { m_highlight = b; update(); }
  void clicked() W_SIGNAL(clicked)

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
  {
    this->setZValue(10);
  }

  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
  {
    this->setZValue(0);
  }
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    clicked();
    event->accept();
  }
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    auto min_dist
        = (event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton))
              .manhattanLength()
          >= QApplication::startDragDistance();
    if (min_dist)
    {
      auto drag = new QDrag{this->parent()};
      QMimeData* mime = new QMimeData;

      auto json = Scenario::copyProcess(m_effect);
      json["Path"] = toJsonObject(score::IDocument::path(m_effect));
      mime->setData(score::mime::effect(), QJsonDocument{json}.toJson());
      drag->setMimeData(mime);

      drag->exec();
      drag->deleteLater();
    }

    event->accept();
  }
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
  }

  const Process::ProcessModel& m_effect;
  QGraphicsItem* m_ui{};
  score::SimpleTextItem* m_label{};
  bool m_highlight{};
};

static void resetInlets(
    Process::ProcessModel& effect,
    const Process::LayerContext& ctx,
    QGraphicsItem* root,
    QObject* parent,
    EffectUi& ui)
{
  qDeleteAll(ui.inlets);
  ui.inlets.clear();
  qreal x = 10;
  auto& portFactory
      = score::AppContext().interfaces<Process::PortFactoryList>();
  for (Process::Inlet* port : effect.inlets())
  {
    if (port->hidden)
      continue;
    Process::PortFactory* fact = portFactory.get(port->concreteKey());
    auto item = fact->makeItem(*port, ctx.context, root, parent);
    item->setPos(x, 7.);
    ui.inlets.push_back(item);

    x += 10.;
  }
  ui.title->setLabelPos(QPointF{x, 0});
}

static void resetOutlets(
    Process::ProcessModel& effect,
    const Process::LayerContext& ctx,
    QGraphicsItem* root,
    QObject* parent,
    EffectUi& ui)
{
  qDeleteAll(ui.outlets);
  ui.outlets.clear();
  qreal x = 10;
  const qreal h = ui.root_item->boundingRect().height() - 10.;
  auto& portFactory
      = score::AppContext().interfaces<Process::PortFactoryList>();
  for (Process::Outlet* port : effect.outlets())
  {
    if (port->hidden)
      continue;
    Process::PortFactory* fact = portFactory.get(port->concreteKey());
    auto item = fact->makeItem(*port, ctx.context, root, parent);
    item->setPos(x, h);
    ui.outlets.push_back(item);

    x += 10.;
  }
}
class View final : public Process::LayerView
{
public:
  explicit View(QGraphicsItem* parent) : Process::LayerView{parent} {}

  void
  setup(const Effect::ProcessModel& object, const Process::LayerContext& ctx)
  {
    auto& doc = ctx.context;
    auto& fact = doc.app.interfaces<Process::LayerFactoryList>();
    auto items = childItems();
    m_effects.clear();
    for (auto item : items)
    {
      this->scene()->removeItem(item);
      delete item;
    }

    double pos_x = 10;
    for (auto& effect : object.effects())
    {
      auto root_item = new score::RectItem(this);
      std::shared_ptr<EffectUi> fx_ui_
          = std::make_shared<EffectUi>(effect, root_item);
      EffectUi& fx_ui = *fx_ui_;

      // Title
      auto title = new EffectTitleItem{effect, object, ctx, this, this, fx_ui};
      fx_ui.title = title;
      fx_ui.title->setParentItem(root_item);


      // Main item
      if (auto factory = fact.findDefaultFactory(effect))
      {
        fx_ui.fx_item = factory->makeItem(effect, doc, root_item);
      }

      if (!fx_ui.fx_item)
      {
        fx_ui.fx_item = new DefaultEffectItem{effect, doc, root_item};
      }

      fx_ui.fx_item->setParentItem(root_item);
      fx_ui.fx_item->setPos({0, 20.});

      /// Rects
      // TODO bind
      m_effects.push_back(fx_ui_);

      auto update_rects = [=] {
        const QRectF cr = fx_ui.root_item->childrenBoundingRect();
        fx_ui.root_item->setRect({0., 0., cr.width(), cr.height() + 15.});
        fx_ui.title->setRect({0., 0., cr.width(), 15.});
      };
      connect(fx_ui.fx_item, &score::ResizeableItem::sizeChanged,
              this, update_rects);

      update_rects();

      fx_ui.root_item->setPos(pos_x, 2);
      pos_x += 10 + fx_ui.root_item->boundingRect().width();

      // Inlets
      resetInlets(effect, ctx, title, this, fx_ui);
      resetOutlets(effect, ctx, title, this, fx_ui);
      fx_ui.cons.push_back(
          con(effect,
              &Process::ProcessModel::inletsChanged,
              this,
              [=, &effect, &ctx, &fx_ui] {
                resetInlets(effect, ctx, title, this, fx_ui);
              }));
      fx_ui.cons.push_back(
          con(effect,
              &Process::ProcessModel::outletsChanged,
              this,
              [=, &effect, &ctx, &fx_ui] {
                resetOutlets(effect, ctx, title, this, fx_ui);
              }));


      connect(
          &effect.selection, &Selectable::changed, root_item, [=](bool ok) {
            root_item->setHighlight(ok);
            title->setHighlight(ok);
          });
    }
  }

  int findDropPosition(QPointF pos) const
  {
    int idx = m_effects.size();
    int i = 0;
    for (const auto& item : m_effects)
    {
      if (pos.x() < item->root_item->pos().x()
                        + item->root_item->boundingRect().width() * 0.5)
      {
        idx = i;
        break;
      }
      i++;
    }
    return idx;
  }

  void setInvalid(bool b)
  {
    m_invalid = b;
    update();
  }

  const std::vector<std::shared_ptr<EffectUi>>& effects() const
  {
    return m_effects;
  }

private:
  void paint_impl(QPainter* p) const override
  {
    if (m_invalid)
    {
      p->fillRect(boundingRect(), Process::Style::instance().AudioPortBrush().brush);
    }

    if (m_lit)
    {
      int idx = *m_lit;

      p->setRenderHint(QPainter::Antialiasing, false);
      p->setPen(Process::Style::instance().TransparentPen());
      p->setBrush(Process::Style::instance().StateDot());
      p->drawRoundedRect(
          QRectF(2.5 + idx * 180, 15, 5, boundingRect().height() - 30), 4, 4);
      p->setRenderHint(QPainter::Antialiasing, false);
    }
  }
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
  {
    if (ev && ev->button() == Qt::RightButton)
    {
      askContextMenu(ev->screenPos(), ev->scenePos());
    }
    else
    {
      pressed(ev->pos());
    }
    ev->accept();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override { ev->accept(); }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override
  {
    ev->accept();
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override
  {
    askContextMenu(ev->screenPos(), ev->scenePos());
    ev->accept();
  }

  void dragEnterEvent(QGraphicsSceneDragDropEvent* ev) override
  {
    dragMoveEvent(ev);
  }
  void dragMoveEvent(QGraphicsSceneDragDropEvent* ev) override
  {
    m_lit = findDropPosition(ev->pos());
    ev->accept();
    update();
  }
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* ev) override
  {
    m_lit = {};
    ev->accept();
    update();
  }
  void dropEvent(QGraphicsSceneDragDropEvent* ev) override
  {
    dropReceived(ev->pos(), *ev->mimeData());
    m_lit = {};
  }

  std::vector<std::shared_ptr<EffectUi>> m_effects;
  bool m_invalid{};
  optional<int> m_lit{};
};

class Presenter final : public Process::LayerPresenter
{
public:
  explicit Presenter(
      const Effect::ProcessModel& model,
      View* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent)
      : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
  {
    putToFront();
    connect(view, &View::pressed, this, [&] {
      m_context.context.focusDispatcher.focus(this);
    });

    connect(
        m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);
    connect(
        m_view,
        &View::dropReceived,
        this,
        [=](const QPointF& pos, const QMimeData& m) {
          int idx = view->findDropPosition(pos);
          on_drop(m, idx);
        });

    auto& m = static_cast<const Effect::ProcessModel&>(model);
    con(m, &Effect::ProcessModel::effectsChanged, this, [&] {
      m_view->setup(
          static_cast<const Effect::ProcessModel&>(model), m_context);
    });
    con(m, &Effect::ProcessModel::badChainingChanged, this, [&](bool b) {
      m_view->setInvalid(b);
    });

    m_view->setup(static_cast<const Effect::ProcessModel&>(model), m_context);
  }

  void setWidth(qreal width, qreal defaultWidth) override { m_view->setWidth(width); }
  void setHeight(qreal val) override { m_view->setHeight(val); }

  void putToFront() override { m_view->setVisible(true); }

  void putBehind() override { m_view->setVisible(false); }

  void on_zoomRatioChanged(ZoomRatio) override {}

  void parentGeometryChanged() override {}

  const Process::ProcessModel& model() const override { return m_layer; }
  const Id<Process::ProcessModel>& modelId() const override
  {
    return m_layer.id();
  }

  void on_drop(const QMimeData& mime, int pos)
  {
    const auto& ctx = context().context;
    if (mime.hasFormat(score::mime::processdata()))
    {
      Mime<Process::ProcessData>::Deserializer des{mime};
      Process::ProcessData p = des.deserialize();

      auto cmd = new InsertEffect(m_layer, p.key, p.customData, pos);
      CommandDispatcher<> d{ctx.commandStack};
      d.submit(cmd);
      return;
    }
    else if (mime.hasFormat(score::mime::effect()))
    {
      auto json
          = QJsonDocument::fromJson(mime.data(score::mime::effect())).object();
      const auto pid = ossia::get_pid();
      const bool same_doc
          = (pid == json["PID"].toInt())
            && (ctx.document.id().val() == json["Document"].toInt());

      if (same_doc)
      {
        const auto old_p
            = fromJsonObject<Path<Process::ProcessModel>>(json["Path"]);
        if (auto obj = old_p.try_find(ctx))
        {
          if (obj->parent() == &m_layer)
          {
            QTimer::singleShot(0, [this, &ctx, id = obj->id(), pos] {
              CommandDispatcher<>{ctx.commandStack}.submit(
                  new MoveEffect(m_layer, id, pos));
            });
            return;
          }
        }
        else
        {
          // todo
          return;
        }
      }
    }
    else if (mime.hasFormat(score::mime::layerdata()))
    {
      QJsonObject json
          = QJsonDocument::fromJson(mime.data(score::mime::layerdata()))
                .object();

      if (json.isEmpty())
        return;
      auto cmd = new LoadEffect(m_layer, json, pos);
      CommandDispatcher<> d{ctx.commandStack};
      d.submit(cmd);
    }
    else if (mime.hasUrls())
    {
      bool all_layers = ossia::all_of(mime.urls(), [](const QUrl& u) {
        return QFileInfo{u.toLocalFile()}.suffix() == "layer";
      });
      if (all_layers)
      {
        auto path = mime.urls().first().toLocalFile();
        if (QFile f{path}; f.open(QIODevice::ReadOnly))
        {
          auto json = QJsonDocument::fromJson(f.readAll()).object();
          if (json.isEmpty())
            return;

          auto cmd = new LoadEffect(m_layer, json, pos);
          CommandDispatcher<> d{ctx.commandStack};
          d.submit(cmd);
        }
      }
      else
      {
        const auto& handlers
            = ctx.app.interfaces<Process::ProcessDropHandlerList>();

        if (auto res = handlers.getDrop(mime, ctx); !res.empty())
        {
          MacroCommandDispatcher<Media::DropEffectMacro> cmd{ctx.commandStack};
          score::Dispatcher_T disp{cmd};
          for (const auto& proc : res)
          {
            auto& p = proc.creation;
            auto create = new InsertEffect(m_layer, p.key, p.customData, pos);
            cmd.submit(create);
            if (auto fx = m_layer.effects().find(create->processId());
                fx != m_layer.effects().list().end())
            {
              if (proc.setup)
              {
                proc.setup(**fx, disp);
              }
            }
          }

          cmd.commit();
        }
      }
    }
  }

private:
  const Effect::ProcessModel& m_layer;
  View* m_view{};
};
}
