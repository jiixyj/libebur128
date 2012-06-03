#include "scanner-drop-qt.moc"

#include <iostream>
#include <glib.h>

#include "logo.h"

gboolean verbose = TRUE;
gboolean histogram = FALSE;
gchar *decode_to_file = NULL;

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint |
                      Qt::WindowSystemMenuHint |
                      Qt::WindowStaysOnTopHint),
      dragPosition(),
      worker_thread_(NULL),
      gui_update_thread_(this),
      logo_rotation_timer(NULL)
{
    setWindowTitle("Loudness Drop");

    setMinimumSize(130, 130);
    setMaximumSize(0, 0);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QAction *quitAction = new QAction(tr("E&xit"), this);
    quitAction->setShortcut(tr("Ctrl+Q"));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    addAction(quitAction);

    setContextMenuPolicy(Qt::ActionsContextMenu);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(0);
    render_area_ = new RenderArea;
    progress_bar_ = new QProgressBar;
    progress_bar_->setFixedHeight(15);
    progress_bar_->setMaximum(130);
    progress_bar_->setTextVisible(false);

    layout->addWidget(render_area_);
    layout->addWidget(progress_bar_);
    setLayout(layout);

    setAcceptDrops(true);

    connect(&gui_update_thread_, SIGNAL(setProgressBar(int)),
            this, SLOT(setProgressBar(int)));
    connect(&gui_update_thread_, SIGNAL(rotateLogo()),
            this, SLOT(rotateLogo()));
    connect(&gui_update_thread_, SIGNAL(resetLogo()),
            this, SLOT(resetLogo()));
    connect(this, SIGNAL(stopGUIThread()),
            &gui_update_thread_, SLOT(stopThread()));
    gui_update_thread_.start();
}

MainWindow::~MainWindow()
{
    emit stopGUIThread();
    gui_update_thread_.wait();
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragPosition = event->globalPos() - frameGeometry().topLeft();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - dragPosition);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setCursor(Qt::ArrowCursor);
        event->accept();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!worker_thread_) {
        worker_thread_ = new WorkerThread(urls);
        worker_thread_->start();
        connect(worker_thread_, SIGNAL(finished()),
                this, SLOT(cleanUpThread()));
        connect(worker_thread_, SIGNAL(showResultList(GSList *, void *)),
                this, SLOT(showResultList(GSList *, void *)));
    }
    event->acceptProposedAction();
}

QSize MainWindow::sizeHint() const
{
    return QSize(130, 130);
}

void MainWindow::cleanUpThread()
{
    worker_thread_->wait();
    delete worker_thread_;
    worker_thread_ = NULL;
}

void MainWindow::setProgressBar(int value)
{
    progress_bar_->setValue(value);
}

void MainWindow::rotateLogo()
{
    logo_rotation_timer = new QTimer;
    connect(logo_rotation_timer, SIGNAL(timeout()),
            render_area_, SLOT(updateLogo()));
    logo_rotation_timer->start(40);
}

void MainWindow::resetLogo()
{
    logo_rotation_timer->stop();
    delete logo_rotation_timer;
    logo_rotation_timer = NULL;
    render_area_->resetLogo();
}

void MainWindow::showResultList(GSList *files, Filetree tree)
{
    ResultWindow *res = new ResultWindow(NULL, files, tree);
    res->setAttribute(Qt::WA_DeleteOnClose);
    res->show();
}

ResultWindow::ResultWindow(QWidget *parent, GSList *files, Filetree tree)
    : QWidget(parent),
      data(files),
      files_(files),
      tree_(tree)
{
    setWindowTitle("Scanning Result");
    QVBoxLayout *layout = new QVBoxLayout;
    setLayout(layout);

    view = new QTreeView;
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(&data);
    proxyModel->setSortRole(Qt::UserRole);
    view->setRootIsDecorated(false);
    view->setAlternatingRowColors(true);
    view->setModel(proxyModel);
    view->setSortingEnabled(true);
    view->sortByColumn(-1);
    view->setItemsExpandable(false);
    view->header()->setResizeMode(QHeaderView::ResizeToContents);
    view->header()->setResizeMode(1, QHeaderView::Stretch);
    view->header()->setStretchLastSection(false);
    view->setItemDelegateForColumn(0, new IconDelegate);

    QHBoxLayout *bbox = new QHBoxLayout;
    QPushButton *close_button = new QPushButton("&Close", this);
    connect(close_button, SIGNAL(clicked()), this, SLOT(close()));
    tag_button = new QPushButton("&Tag files", this);
    connect(tag_button, SIGNAL(clicked()), this, SLOT(tag_files()));
    bbox->addStretch(1);
    bbox->addWidget(tag_button);
    bbox->addWidget(close_button);

    layout->addWidget(view);
    layout->addLayout(bbox);
}

ResultWindow::~ResultWindow()
{
    g_slist_foreach(files_, filetree_free_list_entry, NULL);
    g_slist_free(files_);
    filetree_destroy(tree_);
}

QSize ResultWindow::sizeHint() const
{
    return QSize(800, 400);
}

void ResultWindow::tag_files() {
    struct filename_list_node *fln;
    struct file_data *fd;
    GSList *iter = files_;
    int row_index = 0;
    tag_button->setEnabled(false);
    QCoreApplication::processEvents();
    while (iter) {
        fln = (struct filename_list_node *) iter->data;
        fd = (struct file_data *) fln->d;
        if (fd->scanned) {
            if (!fd->tagged) {
                int ret = 0;
                tag_file(fln, &ret);
                if (!ret) fd->tagged = 1;
                else fd->tagged = 2;
            }
            emit view->dataChanged(proxyModel->mapFromSource(data.index(row_index, 0)),
                                   proxyModel->mapFromSource(data.index(row_index, 0)));
            QCoreApplication::processEvents();
            ++row_index;
        }
        iter = g_slist_next(iter);
    }
}

ResultData::ResultData(GSList *files)
{
    GSList *it = files;
    while (it) {
        struct filename_list_node *fln = (struct filename_list_node *) it->data;
        struct file_data *fd = (struct file_data *) fln->d;
        if (fd->scanned) {
            files_.push_back(fln);
        }
        it = g_slist_next(it);
    }
}

int ResultData::rowCount(const QModelIndex &) const
{
    return int(files_.size());
}

int ResultData::columnCount(const QModelIndex &) const
{
    return 6;
}

QVariant ResultData::data(QModelIndex const& index, int role) const
{
    if (role == Qt::DisplayRole) {
        struct filename_list_node *fln = files_[size_t(index.row())];
        struct file_data *fd = (struct file_data *) fln->d;
        switch (index.column()) {
        case 1:
            return fln->fr->display;
        case 2: {
            gchar *p = g_strdup_printf("%+.2f dB", fd->gain_album);
            QVariant r(p);
            g_free(p);
            return r;
        }
        case 3: {
            gchar *p = g_strdup_printf("%+.2f dB",
                           clamp_rg(RG_REFERENCE_LEVEL - fd->loudness));
            QVariant r(p);
            g_free(p);
            return r;
        }
        case 4: {
            gchar *p = g_strdup_printf("%.6f", fd->peak_album);
            QVariant r(p);
            g_free(p);
            return r;
        }
        case 5: {
            gchar *p = g_strdup_printf("%.6f", fd->peak);
            QVariant r(p);
            g_free(p);
            return r;
        }
        default:
            return QVariant();
        }
    } else if (role == Qt::UserRole) {
        struct filename_list_node *fln = files_[size_t(index.row())];
        struct file_data *fd = (struct file_data *) fln->d;
        switch (index.column()) {
        case 0:
            return fd->tagged;
        case 1:
            return fln->fr->collate_key;
        case 2:
            return fd->gain_album;
        case 3:
            return clamp_rg(RG_REFERENCE_LEVEL - fd->loudness);
        case 4:
            return fd->peak_album;
        case 5:
            return fd->peak;
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QVariant ResultData::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0:
                return tr("Tagged");
            case 1:
                return tr("File");
            case 2:
                return tr("Album Gain");
            case 3:
                return tr("Track Gain");
            case 4:
                return tr("Album Peak");
            case 5:
                return tr("Track Peak");
            default:
                return QVariant();
        }
    }
    return QVariant();
}

IconDelegate::IconDelegate(QWidget *parent)
    : QStyledItemDelegate(parent)
{}

void IconDelegate::paint(QPainter *painter, QStyleOptionViewItem const& option,
                         QModelIndex const& index) const
{
    int tag_status = qVariantValue<int>(index.data(Qt::UserRole));
    QStyledItemDelegate::paint(painter, option, index);
    if (tag_status == 0) return;
    QIcon icon = tag_status == 1
           ? QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton)
           : QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton);
    QRect paint_rect = option.rect;
    if (paint_rect.height() > 16) {
        int diff = paint_rect.height() - 16;
        paint_rect.setHeight(16);
        paint_rect.moveTop(paint_rect.top() + diff / 2);
    }
    icon.paint(painter, paint_rect, option.decorationAlignment);
}


RenderArea::RenderArea(QWidget *parent)
    : QWidget(parent),
      svg_renderer_(),
      rotation_state(0)
{
    QByteArray logo((const char *) test_svg, int(test_svg_len));
    svg_renderer_ = new QSvgRenderer(logo, this);
}

void RenderArea::updateLogo()
{
    rotation_state += G_PI / 20;
    if (rotation_state >= 2.0 * G_PI) rotation_state = 0.0;
    repaint();
}

void RenderArea::resetLogo()
{
    rotation_state = 0.0;
    repaint();
}

void RenderArea::paintEvent(QPaintEvent *)
{
    static const qreal scale_factor = 0.8f;
    QPainter painter(this);
    QSize s = svg_renderer_->defaultSize();
    QSizeF ws(size());
    qreal svg_aspect = qreal(s.width()) / qreal(s.height());
    qreal canvas_aspect = ws.width() / ws.height();

    painter.translate( ws.width() / 2.0,  ws.height() / 2.0);
    painter.rotate(rotation_state / G_PI * 180.0);
    painter.translate(-ws.width() / 2.0, -ws.height() / 2.0);
    QSizeF ns = ws;
    ns.setWidth(ws.width() * svg_aspect / canvas_aspect * scale_factor);
    ns.setHeight(ws.height() * scale_factor);
    painter.translate((ws.width() - ns.width()) / 2.0,
                      (ws.height() - ns.height()) / 2.0);

    painter.scale(svg_aspect / canvas_aspect * scale_factor, scale_factor);

    svg_renderer_->render(&painter);
}

WorkerThread::WorkerThread(QList<QUrl> const& urls)
    : QThread(NULL),
      urls_(urls)
{}

void WorkerThread::run()
{
    std::vector<char *> roots;
    for (QList<QUrl>::ConstIterator it = urls_.begin(); it != urls_.end(); ++it) {
#ifdef G_OS_WIN32
        roots.push_back(g_strdup(it->toLocalFile().toUtf8().constData()));
#else
        roots.push_back(g_strdup(it->toLocalFile().toLocal8Bit().constData()));
#endif
    }
    GSList *errors = NULL, *files = NULL;
    Filetree tree = filetree_init(&roots[0], roots.size(), TRUE, FALSE, FALSE, &errors);

    g_slist_foreach(errors, filetree_print_error, &verbose);
    g_slist_foreach(errors, filetree_free_error, NULL);
    g_slist_free(errors);

    filetree_file_list(tree, &files);
    filetree_remove_common_prefix(files);

    int result = scan_files(files);
    if (result) {
        emit showResultList(files, tree);
    } else {
        g_slist_foreach(files, filetree_free_list_entry, NULL);
        g_slist_free(files);
        filetree_destroy(tree);
    }
}

GUIUpdateThread::GUIUpdateThread(QWidget *parent)
    : QThread(parent),
      old_progress_bar_value_(0),
      rotation_active_(false),
      stop_thread_(false)
{}

void GUIUpdateThread::run()
{
    for (;;) {
        g_mutex_lock(progress_mutex);
        g_cond_wait(progress_cond, progress_mutex);
        if (stop_thread_) {
            g_mutex_unlock(progress_mutex);
            break;
        }
        if (total_frames > 0) {
            if (!rotation_active_ && elapsed_frames) {
                rotation_active_ = true;
                emit rotateLogo();
            }
            int new_value = int(CLAMP(double(elapsed_frames) /
                                      double(total_frames), 0.0, 1.0)
                              * 130.0 + 0.5);
            if (new_value != old_progress_bar_value_) {
                emit setProgressBar(new_value);
                old_progress_bar_value_ = new_value;
            }
            if (total_frames == elapsed_frames) {
                emit setProgressBar(0);
                old_progress_bar_value_ = 0;
                rotation_active_ = false;
                emit resetLogo();
            }
        }
        g_mutex_unlock(progress_mutex);
    }
}

void GUIUpdateThread::stopThread()
{
    stop_thread_ = true;
    while (isRunning()) {
        g_cond_broadcast(progress_cond);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    /* initialization */
    g_thread_init(NULL);
    input_init(argv[0], NULL);
    scanner_init_common();
    setlocale(LC_COLLATE, "");
    setlocale(LC_CTYPE, "");
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));

    MainWindow window;
    window.show();
    return app.exec();
}
