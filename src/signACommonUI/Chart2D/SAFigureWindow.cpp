﻿#include "SAFigureWindow.h"
#include <QtWidgets/QApplication>
#include <QMessageBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QAction>
#include <QMimeData>
#include <QPaintEvent>
#include <QCoreApplication>
#include <QScopedPointer>
//sa chart
#include "SAChart2D.h"
#include "SAQwtSerialize.h"
//sa lib
#include "SAData.h"
#include "SARandColorMaker.h"
#include "SAFigureGlobalConfig.h"
#include "SAValueManager.h"
#include "SAValueManagerMimeData.h"
//sa common ui
#include "SAFigureContainer.h"
#include "SAFigureChartRubberbandEditOverlay.h"
#define GET_CHART_PTR \
SAChart2D* chart = current2DPlot();\
if(nullptr == chart)\
{\
    return nullptr;\
}

#define GET_CHART_PTR_RETURN_VOID \
SAChart2D* chart = current2DPlot();\
if(nullptr == chart)\
{\
    return;\
}



class SAFigureWindowPrivate
{
    SA_IMPL_PUBLIC(SAFigureWindow)
public:
    SAFigureContainer *centralwidget;
    SAChart2D *currentPlot;
    SAFigureChartRubberbandEditOverlay* chartRubberbandEditor;///< 编辑模式
    SAFigureWindowPrivate(SAFigureWindow* p):q_ptr(p)
      ,centralwidget(nullptr)
      ,currentPlot(nullptr)
      ,chartRubberbandEditor(nullptr)
    {

    }

    void setupUI()
    {
        centralwidget = new SAFigureContainer(q_ptr);
        centralwidget->setObjectName(QStringLiteral("centralwidget"));
        q_ptr->setCentralWidget(centralwidget);
        q_ptr->setAcceptDrops(true);
        q_ptr->setAutoFillBackground(true);
        q_ptr->setBackgroundColor(QColor(255,255,255));
        q_ptr->setWindowIcon(QIcon(":/icon/icons/figureWindow.png"));
    }

    void retranslateUi()
    {
        q_ptr->setWindowTitle(QApplication::translate("SAFigureWindow", "Figure", 0));
    }
};

//=============================================================================

SAFigureWindow::SAFigureWindow(QWidget *parent) :
    QMainWindow(parent)
    ,d_ptr(new SAFigureWindowPrivate(this))
{
    d_ptr->setupUI();
    setFocusPolicy(Qt::ClickFocus);
    static int s_figure_count=0;
    ++s_figure_count;
    setWindowTitle(QString("figure-%1").arg(s_figure_count));
    setMinimumWidth(100);
    setMinimumHeight(50);
}

SAFigureWindow::~SAFigureWindow()
{
    //qDebug() << "SAFigureWindow destroy";
}

QList<QWidget*> SAFigureWindow::getWidgets() const
{
    return d_ptr->centralwidget->getWidgetList();
}

///
/// \brief 添加一个2Dchart
/// \return 返回2D绘图的指针
///
SAChart2D *SAFigureWindow::create2DPlot()
{
    return create2DSubPlot(0.05,0.05,0.9,0.9);
}

SAChart2D *SAFigureWindow::create2DSubPlot(float xPresent, float yPresent, float wPresent, float hPresent)
{
    try
    {
        SAChart2D* plot = new SAChart2D(d_ptr->centralwidget);
        d_ptr->centralwidget->addWidget (plot,xPresent,yPresent,wPresent,hPresent);
        d_ptr->currentPlot = plot;
        emit chartAdded(plot);
        setFocusProxy(plot);
        return plot;
    }
    catch(std::bad_alloc& b)
    {
        Q_UNUSED(b);
        QMessageBox::warning (this,tr("memory out"),tr("memory out"));
        return nullptr;
    }
    return nullptr;
}

///
/// \brief 获取所有的图像列表
/// \return
///
QList<SAChart2D *> SAFigureWindow::get2DPlots() const
{
    QList<SAChart2D*> res;
    QList<QWidget *> widgets = d_ptr->centralwidget->getWidgetList();
    for(auto i=widgets.begin();i!=widgets.end();++i)
    {
        SAChart2D* chart = qobject_cast<SAChart2D*>(*i);
        if(chart)
        {
            res.append(chart);
        }
    }
    return res;
}

///
/// \brief 当前的2d绘图指针
/// \return 当没有2d绘图时返回nullptr
///
SAChart2D *SAFigureWindow::current2DPlot() const
{
    return d_ptr->currentPlot;
}

void SAFigureWindow::clearAll()
{
    QList<SAChart2D*> charts = get2DPlots();
    while(!charts.isEmpty())
    {
        SAChart2D* p = charts.takeLast();
        emit chartRemoved(p);
        delete p;
    }
}




///
/// \brief 设置窗体背景
/// \param brush
///
void SAFigureWindow::setBackgroundColor(const QBrush &brush)
{
    if(!autoFillBackground())
        setAutoFillBackground(true);
    QPalette p = palette();
    p.setBrush(QPalette::Window,brush);
    setPalette(p);
}

void SAFigureWindow::setBackgroundColor(const QColor &clr)
{
    if(!autoFillBackground())
        setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Window,clr);
    setPalette(p);
}
///
/// \brief 获取窗口的位置
/// \param w
/// \return
///
QRectF SAFigureWindow::getWidgetPos(QWidget *w) const
{
    return d_ptr->centralwidget->getWidgetPos(w);
}

///
/// \brief 设置当前的2dplot
/// \param p
///
void SAFigureWindow::setCurrent2DPlot(SAChart2D *p)
{
    d_ptr->currentPlot = p;
    setFocusProxy(p);
}
///
/// \brief 通过item查找对应的SAChart2D，如果没有返回nullptr
/// \param item
/// \return 如果没有返回nullptr
///
SAChart2D *SAFigureWindow::findChartFromItem(QwtPlotItem *item)
{
    QList<SAChart2D*> charts = get2DPlots();
    const int chartSize = charts.size();
    for(int i=0;i<chartSize;++i)
    {
        QwtPlotItemList items = charts[i]->itemList();
        if(items.contains(item))
            return charts[i];
    }
    return nullptr;
}
///
/// \brief 是否开始子窗口编辑模式
/// \param enable
/// \param ptr 通过此参数可以指定自定义的编辑器，若为nullptr，将使用默认的编辑器，此指针的管理权将移交SAFigureWindow
///
void SAFigureWindow::enableSubWindowEditMode(bool enable,SAFigureChartRubberbandEditOverlay* ptr)
{
    if(enable)
    {
        if(nullptr == d_ptr->chartRubberbandEditor)
        {
            if(ptr)
            {
                d_ptr->chartRubberbandEditor = ptr;
                d_ptr->chartRubberbandEditor->show();
            }
            else
            {
                d_ptr->chartRubberbandEditor = new SAFigureChartRubberbandEditOverlay(this);
                d_ptr->chartRubberbandEditor->show();
            }
        }
        else
        {
            if(d_ptr->chartRubberbandEditor->isHidden())
            {
                d_ptr->chartRubberbandEditor->show();
            }
        }
    }
    else
    {
        if(d_ptr->chartRubberbandEditor)
        {
            delete d_ptr->chartRubberbandEditor;
            d_ptr->chartRubberbandEditor = nullptr;
        }
    }
}
///
/// \brief 获取子窗口编辑器指针，若没有此编辑器，返回nullptr
///
/// 此指针的管理权在SAFigureWindow上，不要在外部对此指针进行释放
/// \return
///
SAFigureChartRubberbandEditOverlay *SAFigureWindow::subWindowEditModeOverlayWidget() const
{
    return d_ptr->chartRubberbandEditor;
}



//void SAFigureWindow::dragEnterEvent(QDragEnterEvent *event)
//{
//    qDebug() << "SAFigureWindow dragEnterEvent mimeData:"<<event->mimeData()->formats();
//    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
//    {
//        //event->setDropAction(Qt::MoveAction);
//        event->acceptProposedAction();
//        qDebug() << "SAFigureWindow dragEnterEvent acceptProposedAction";
//        return;
//    }
////    else
////    {
////        qDebug() << "SAFigureWindow dragEnterEvent ignore";
////        event->ignore();
////    }
//    QMainWindow::dragEnterEvent(event);
//}

//void SAFigureWindow::dragMoveEvent(QDragMoveEvent *event)
//{
//    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
//    {
////        event->setDropAction(Qt::MoveAction);
////        event->accept();
//        event->acceptProposedAction();
//        qDebug() << "SAFigureWindow dragMoveEvent acceptProposedAction";
//        return;
//    }
//    QMainWindow::dragMoveEvent(event);
//}

//void SAFigureWindow::dropEvent(QDropEvent *event)
//{
//    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
//    {
//        qDebug() << "SAFigureWindow::dropEvent";
//        QList<int> ids;
//        if(SAValueManagerMimeData::getValueIDsFromMimeData(event->mimeData(),ids))
//        {
//            QList<SAAbstractDatas*> datas = saValueManager->findDatas(ids);
//            if(SAChart2D * c = current2DPlot())
//            {
//                for(int i=0;i<datas.size();++i)
//                {
//                    c->addCurve(datas[i]);
//                }
//            }
//        }
//    }
//}

void SAFigureWindow::redo()
{
    SAChart2D * c = current2DPlot();
    if(c)
    {
        c->redo();
    }
}

void SAFigureWindow::undo()
{
    SAChart2D * c = current2DPlot();
    if(c)
    {
        c->undo();
    }
}

#if 0
void SAFigureWindow::keyPressEvent(QKeyEvent *e)
{
    qDebug() << e->type() << e->key() << " " << e->modifiers();
    if(Qt::ControlModifier & e->modifiers())
    {
        if(Qt::Key_Z == e->key())
        {
            undo();
        }
        else if(Qt::Key_Y == e->key())
        {
            redo();
        }
    }
    QMainWindow::keyPressEvent(e);
}
#endif








QDataStream &operator <<(QDataStream &out, const SAFigureWindow *p)
{
    const int magicStart = 0x1314abc;
    out << magicStart
        << p->saveGeometry()
        << p->saveState()
           ;
    QList<SAChart2D*> charts = p->get2DPlots();
    QList<QRectF> pos;
    for(int i=0;i<charts.size();++i)
    {
        pos.append(p->getWidgetPos(charts[i]));
    }
    out << pos;
    for(int i=0;i<charts.size();++i)
    {
        out << charts[i];
    }
    return out;
}

QDataStream &operator >>(QDataStream &in, SAFigureWindow *p)
{
    const int magicStart = 0x1314abc;
    int tmp;
    in >> tmp;
    if(tmp != magicStart)
    {
        throw sa::SABadSerializeExpection();
        return in;
    }
    QByteArray geometryData,stateData;
    in >> geometryData
            >> stateData
            ;
    p->restoreGeometry(geometryData);
    p->restoreState(stateData);
    QList<QRectF> pos;
    in >> pos;
    try
    {
        for(int i=0;i<pos.size();++i)
        {
            const QRectF& r = pos[i];
            QScopedPointer<SAChart2D> chart(p->create2DSubPlot(r.x(),r.y(),r.width(),r.height()));
            in >> chart.data();
            chart->show();
            chart.take();
        }
    }
    catch(const sa::SABadSerializeExpection& exp)
    {
        throw exp;
    }
    return in;
}
