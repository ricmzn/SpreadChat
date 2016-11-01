#include "ChatWindow.h"
#include "ui_ChatWindow.h"

#include <QMenu>
#include <QToolBar>
#include <QMessageBox>
#include <QComboBox>
#include <QTextCodec>
#include <QCompleter>
#include "JoinDialog.h"
#include "ChatTabWidget.h"
#include "SpreadConnection.h"

Q_DECLARE_METATYPE(QTextCodec*);

static QList<QTextCodec*> getSystemCodecs()
{
    QList<int> codecMibs = QTextCodec::availableMibs();
    QList<QTextCodec*> codecs;
    codecs.reserve(codecMibs.size());
    for(int mib : codecMibs) {
        codecs.push_back(QTextCodec::codecForMib(mib));
    }
    qSort(codecs.begin(), codecs.end(), [](const QTextCodec* a, const QTextCodec* b) {
        return a->name().toLower() < b->name().toLower();
    });
    return codecs;
}

ChatWindow::ChatWindow(SpreadConnPtr conn, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::ChatWindow)
    , connection(std::move(conn))
    , defaultTabVisible(false)
{
    ui->setupUi(this);
    QToolBar* toolbar = ui->toolBar;
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    inCodec = new QComboBox(this);
    inCodec->setMaxVisibleItems(100);
    inCodec->setMaximumWidth(120);

    outCodec = new QComboBox(this);
    outCodec->setMaxVisibleItems(100);
    outCodec->setMaximumWidth(120);

    QList<QTextCodec*> codecs = getSystemCodecs();
    for(QTextCodec* codec : codecs) {
        QVariant codecProxy = QVariant::fromValue<QTextCodec*>(codec);
        inCodec->addItem(codec->name(), codecProxy);
        outCodec->addItem(codec->name(), codecProxy);
    }

    inCodec->setCurrentText("UTF-16");
    inCodec->setToolTip("Codificação da entrada");
    outCodec->setCurrentText("UTF-16");
    outCodec->setToolTip("Codificação da saída");

    toolbar->addWidget(spacer);
    toolbar->addWidget(inCodec);
    toolbar->addWidget(outCodec);

    setWindowTitle(QString("Chat em %1").arg(connection->getHostname()));
    createDefaultTab();

    connect(connection->getWorker(), SIGNAL(messageReceived(SpreadMessage)), this, SLOT(receiveMessage(SpreadMessage)));
}

ChatWindow::~ChatWindow()
{
    delete ui;
}

QTextCodec* ChatWindow::getInputEncoding()
{
    return inCodec->currentData().value<QTextCodec*>();
}

QTextCodec* ChatWindow::getOutputEncoding()
{
    return outCodec->currentData().value<QTextCodec*>();
}

void ChatWindow::on_actionAbout_triggered()
{
    const static QString versionString = QString("%1 %2").arg(
        "Este produto utiliza software desenvolvido pela Spread "
        "Concepts LLC para uso junto ao Spread toolkit. Para mais informações sobre o "
        "Spread, visite <a href='http://www.spread.org/'>http://www.spread.org/</a><br>"
        "<hr><br>"
        "Versão do Spread: "
    )
    .arg(SpreadConnection::getVersion());

    QMessageBox about(this);
    about.setWindowTitle("Sobre");
    about.setTextFormat(Qt::RichText);
    about.setIcon(QMessageBox::Information);
    about.setText(versionString);
    about.exec();
}

void ChatWindow::on_actionJoinGroup_triggered()
{
    JoinDialog dialog;
    while (dialog.exec()) {
        std::string groupName = dialog.groupName().toStdString();
        if (!connection->inGroup(groupName.data())) {
            // Entra no grupo novo
            SpreadGroup* group = connection->joinGroup(groupName.data());
            if (group) {
                addGroupTab(group);
                break;
            }
            else {
                QMessageBox::critical(this, "Erro", "Nome de grupo inválido", QMessageBox::Ok);
            }
        }
        else {
            // Seleciona o grupo existente
            int index = 0;
            while (index < ui->tabWidget->count()) {
                ChatTabWidget* widget = dynamic_cast<ChatTabWidget*>(ui->tabWidget->widget(index));
                auto name = widget->getGroup()->getName();
                if (name == groupName.data()) {
                    break;
                }
                index++;
            }
            ui->tabWidget->setCurrentIndex(index);
            break;
        }
    }
}

void ChatWindow::on_tabWidget_tabCloseRequested(int index)
{
    ChatTabWidget* widget = dynamic_cast<ChatTabWidget*>(ui->tabWidget->widget(index));
    const SpreadGroup* group = widget->getGroup();
    auto tab = tabs.find(group->getName());
    tabs.erase(tab);
    connection->leaveGroup(group);
    ui->tabWidget->removeTab(index);
    if (ui->tabWidget->count() == 0) {
        createDefaultTab();
    }
}

void ChatWindow::receiveMessage(SpreadMessage message)
{
    auto tab = tabs.find(message.group);
    tab->second->addMessage(message);
}

void ChatWindow::addGroupTab(SpreadGroup* group)
{
    if (defaultTabVisible) {
        ui->tabWidget->removeTab(0);
        ui->tabWidget->setEnabled(true);
        defaultTabVisible = false;
    }
    auto widget = new ChatTabWidget(group, this);
    ui->tabWidget->addTab(widget, group->getName());
    ui->tabWidget->setCurrentIndex(ui->tabWidget->count() - 1);
    tabs.insert({group->getName(), widget});
    widget->setFocus();
}

void ChatWindow::createDefaultTab()
{
    ui->tabWidget->addTab(new QWidget(this), "Nenhum grupo");
    ui->tabWidget->setCurrentIndex(0);
    ui->tabWidget->setEnabled(false);
    defaultTabVisible = true;
}
