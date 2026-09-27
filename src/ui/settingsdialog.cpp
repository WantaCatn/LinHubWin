#include "ui/settingsdialog.h"
#include "app/appsettings.h"
#include "storage/commandrepository.h"
#include "storage/databasemanager.h"
#include "storage/historyrepository.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    resize(560, 460);
    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;

    auto *appear = new QWidget;
    auto *af = new QFormLayout(appear);
    auto *theme = new QComboBox;
    for (const QString &id : AppSettings::themeIds())
        theme->addItem(AppSettings::themeDisplayName(id), id);
    const int ti = theme->findData(AppSettings::instance().uiTheme());
    if (ti >= 0)
        theme->setCurrentIndex(ti);

    auto *uiFont = new QFontComboBox;
    uiFont->setCurrentFont(AppSettings::instance().uiFont());
    auto *uiSize = new QSpinBox;
    uiSize->setRange(8, 20);
    uiSize->setValue(AppSettings::instance().uiFontSize());
    auto *termFont = new QFontComboBox;
    termFont->setFontFilters(QFontComboBox::MonospacedFonts);
    const QFont tf = AppSettings::instance().termFont();
    termFont->setCurrentFont(tf);
    auto *termSize = new QSpinBox;
    termSize->setRange(8, 32);
    termSize->setValue(AppSettings::instance().termFontSize());

    auto *showQuick = new QCheckBox(QStringLiteral("显示顶部快速连接栏"));
    showQuick->setChecked(AppSettings::instance().showQuickConnect());
    auto *showCompose = new QCheckBox(QStringLiteral("显示底部发送 / 快速命令栏"));
    showCompose->setChecked(AppSettings::instance().showComposeBar());
    auto *showTb = new QCheckBox(QStringLiteral("显示主工具栏"));
    showTb->setChecked(AppSettings::instance().showToolbar());
    auto *sbLines = new QSpinBox;
    sbLines->setRange(200, 20000);
    sbLines->setSingleStep(200);
    sbLines->setValue(AppSettings::instance().scrollbackLines());
    sbLines->setSuffix(QStringLiteral(" 行"));
    auto *logHi = new QCheckBox(QStringLiteral("日志关键字着色（error / warn / info）"));
    logHi->setChecked(AppSettings::instance().logHighlight());
    auto *confirmDis = new QCheckBox(QStringLiteral("断开连接时弹出确认（可勾选不再提示）"));
    confirmDis->setChecked(AppSettings::instance().confirmDisconnect());
    auto *promptEdit = new QCheckBox(QStringLiteral("SFTP 编辑保存后询问是否上传到服务器"));
    promptEdit->setChecked(AppSettings::instance().promptEditUpload());
    auto *askImage = new QCheckBox(QStringLiteral("打开图片时询问是否使用看图工具"));
    askImage->setChecked(AppSettings::instance().askImageOpen());

    af->addRow(QStringLiteral("界面主题"), theme);
    af->addRow(QStringLiteral("界面字体"), uiFont);
    af->addRow(QStringLiteral("界面字号"), uiSize);
    af->addRow(QStringLiteral("终端字体"), termFont);
    af->addRow(QStringLiteral("终端字号"), termSize);
    af->addRow(QString(), showQuick);
    af->addRow(QString(), showCompose);
    af->addRow(QString(), showTb);
    af->addRow(QStringLiteral("终端回滚行数"), sbLines);
    af->addRow(QString(), logHi);
    af->addRow(QString(), confirmDis);
    af->addRow(QString(), promptEdit);
    af->addRow(QString(), askImage);
    auto *hint = new QLabel(QStringLiteral("终端内也可使用 Ctrl + / Ctrl - / Ctrl 0 调整字号。回滚行数越小越省内存。"));
    hint->setWordWrap(true);
    af->addRow(hint);
    tabs->addTab(appear, QStringLiteral("外观"));

    auto *data = new QWidget;
    auto *df = new QFormLayout(data);
    df->addRow(QStringLiteral("数据库"), new QLabel(DatabaseManager::instance().filePath()));
    df->addRow(QStringLiteral("历史条数"), new QLabel(QString::number(HistoryRepository().count())));
    auto *days = new QSpinBox;
    days->setRange(0, 3650);
    days->setValue(0);
    days->setSpecialValueText(QStringLiteral("不清理"));
    df->addRow(QStringLiteral("清理 N 天前历史"), days);
    auto *clean = new QPushButton(QStringLiteral("立即清理历史"));
    connect(clean, &QPushButton::clicked, this, [this, days] {
        if (days->value() <= 0)
            return;
        HistoryRepository().removeOlderThanDays(days->value());
        QMessageBox::information(this, QStringLiteral("LinHub"), QStringLiteral("已按天数清理连接历史。"));
    });
    auto *addCmd = new QPushButton(QStringLiteral("新增快速命令"));
    connect(addCmd, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("快速命令"),
                                                   QStringLiteral("名称"), QLineEdit::Normal, {}, &ok);
        if (!ok || name.isEmpty())
            return;
        const QString cmd = QInputDialog::getMultiLineText(this, QStringLiteral("快速命令"),
                                                           QStringLiteral("命令内容"), {}, &ok);
        if (!ok || cmd.isEmpty())
            return;
        QuickCommand c;
        c.name = name;
        c.command = cmd;
        CommandRepository().insert(c);
    });
    df->addRow(clean);
    df->addRow(addCmd);
    tabs->addTab(data, QStringLiteral("数据 / 命令"));

    root->addWidget(tabs);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, this, [this, theme, uiFont, uiSize, termFont, termSize,
                                                      showQuick, showCompose, showTb, sbLines, logHi,
                                                      confirmDis, promptEdit, askImage] {
        auto &st = AppSettings::instance();
        st.setUiTheme(theme->currentData().toString());
        st.setUiFont(uiFont->currentFont().family(), uiSize->value());
        st.setTermFont(termFont->currentFont().family(), termSize->value());
        st.setShowQuickConnect(showQuick->isChecked());
        st.setShowComposeBar(showCompose->isChecked());
        st.setShowToolbar(showTb->isChecked());
        st.setScrollbackLines(sbLines->value());
        st.setLogHighlight(logHi->isChecked());
        st.setConfirmDisconnect(confirmDis->isChecked());
        st.setPromptEditUpload(promptEdit->isChecked());
        st.setAskImageOpen(askImage->isChecked());
        AppSettings::applyAppearance();
        emit appearanceChanged();
        accept();
    });
    root->addWidget(box);
}
