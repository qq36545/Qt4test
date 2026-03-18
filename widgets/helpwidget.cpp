#include "helpwidget.h"
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QLabel>
#include <QResizeEvent>
#include <QEvent>
#include <QPalette>
#include <QTextDocument>
#include <algorithm>

HelpWidget::HelpWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void HelpWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(0);

    // 创建内容容器
    contentWidget = new QWidget();
    contentWidget->setObjectName("helpContentWidget");

    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(40, 40, 40, 40);
    contentLayout->setSpacing(20);

    // 标题
    titleLabel = new QLabel("使用前必看");
    titleLabel->setObjectName("helpTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 28px; font-weight: 700;");

    // 内容文本浏览器
    textBrowser = new QTextBrowser();
    textBrowser->setObjectName("helpTextBrowser");
    textBrowser->setOpenExternalLinks(true);
    textBrowser->setFrameShape(QFrame::NoFrame);

    // 设置 HTML 内容
    QString htmlContent = R"(
        <div style="font-size: 16px; line-height: 1.8; text-align: left;">
            <h2 style="color: #4fc3f7; margin-top: 20px; text-align: left;">初次使用本软件需要做的两件事情：</h2>
            <ol style="margin-left: 20px; text-align: left;">
                <li style="margin-bottom: 15px;">
                    配置AI模型的密钥，点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/E9YedBOaGoaqS0xvx3qcTrG3n4e?from=from_copylink" style="color: #4fc3f7; text-decoration: none;">这里</a>
                </li>
                <li style="margin-bottom: 15px;">
                    配置临时图床的密钥，点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/WBlkdfh30otuWTxvhhucazUpnyf?from=from_copylink" style="color: #4fc3f7; text-decoration: none;">这里</a>
                </li>
            </ol>

            <p style="margin-top: 30px; font-size: 16px;">
                如有任何问题，加我微信 <strong style="color: #4fc3f7;">312088415</strong> 拉你进群一起交流学习！
            </p>

            <p style="margin-top: 20px;">
                更多使用教程集合点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/TBeZdghZhoG9yzxju06cTxFBntg?from=from_copylink" style="color: #4fc3f7; text-decoration: none;">这里</a>
            </p>
        </div>
    )";
    textBrowser->setHtml(htmlContent);

    // 深浅主题分别应用两套文字颜色
    updateThemeStyles();

    // 添加到布局
    contentLayout->addWidget(titleLabel, 0, Qt::AlignHCenter);
    contentLayout->addWidget(textBrowser);
    contentLayout->addStretch();

    // 居中对齐
    mainLayout->addStretch();
    mainLayout->addWidget(contentWidget, 0, Qt::AlignCenter);
    mainLayout->addStretch();

    // 初始尺寸：宽度保持 50%，文本框高度增至 tab 页 100%
    const int contentWidth = std::max(520, width() / 2);
    const int contentHeight = height();
    contentWidget->setFixedSize(contentWidth, contentHeight);
}

void HelpWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // 内容框：宽度 50%，文本框高度增至 tab 页 100%
    const int contentWidth = std::max(520, width() / 2);
    const int contentHeight = height();
    contentWidget->setFixedSize(contentWidth, contentHeight);
}

void HelpWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        updateThemeStyles();
    }
}

void HelpWidget::updateThemeStyles()
{
    // MainWindow 是通过 setStyleSheet() 应用主题，不在 qApp 上。
    // 这里必须从顶层窗口读取样式，否则会误判为浅色主题。
    const QWidget *topLevel = window();
    const QString hostStyle = topLevel ? topLevel->styleSheet() : styleSheet();
    const bool isDarkTheme = hostStyle.contains("background: #000000")
        || hostStyle.contains("stop:0 #000000");

    if (isDarkTheme) {
        titleLabel->setStyleSheet("font-size: 28px; font-weight: 700; color: #ffffff;");
        textBrowser->setStyleSheet("QTextBrowser { color: #ffffff; background: transparent; }");
        textBrowser->document()->setDefaultStyleSheet("div, p, li, ol { color: #ffffff; }");
    } else {
        titleLabel->setStyleSheet("font-size: 28px; font-weight: 700; color: #111111;");
        textBrowser->setStyleSheet("QTextBrowser { color: #111111; background: transparent; }");
        textBrowser->document()->setDefaultStyleSheet("div, p, li, ol { color: #111111; }");
    }
}
