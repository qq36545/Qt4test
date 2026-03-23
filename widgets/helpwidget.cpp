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

void HelpWidget::refreshThemeStyles()
{
    updateThemeStyles();
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
    titleLabel->setStyleSheet("font-size: 30px; font-weight: 700;");

    // 内容文本浏览器
    textBrowser = new QTextBrowser();
    textBrowser->setObjectName("helpTextBrowser");
    textBrowser->setOpenExternalLinks(true);
    textBrowser->setFrameShape(QFrame::NoFrame);

    // 设置 HTML 内容
    QString htmlContent = R"(
        <div class="help-root">
            <h2>🚀 快速开始</h2>
            <p>首次使用建议先完成下面两步配置，成功后即可开始批量生成。</p>
            <hr/>

            <h3>✅ 必做两步</h3>
            <ol>
                <li>
                    配置 AI 模型密钥：点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/E9YedBOaGoaqS0xvx3qcTrG3n4e?from=from_copylink">这里</a>
                </li>
                <li>
                    配置临时图床密钥：点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/WBlkdfh30otuWTxvhhucazUpnyf?from=from_copylink">这里</a>
                </li>
            </ol>
            <hr/>

            <h3>💬 常见支持</h3>
            <p>遇到问题可添加微信 <strong>312088415</strong>，拉你进群一起交流学习。</p>
            <hr/>

            <h3>📚 更多教程</h3>
            <p>完整教程集合：点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/TBeZdghZhoG9yzxju06cTxFBntg?from=from_copylink">这里</a></p>
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

    // 初始尺寸：在原基础上放大一倍（宽度 100%，高度 200%）
    const int contentWidth = std::max(1040, width());
    const int contentHeight = std::max(800, height() * 2);
    contentWidget->setFixedSize(contentWidth, contentHeight);
}

void HelpWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // 内容框：在原基础上放大一倍（宽度 100%，高度 200%）
    const int contentWidth = std::max(1040, width());
    const int contentHeight = std::max(800, height() * 2);
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
    const QWidget *topLevel = window();
    QString theme = topLevel ? topLevel->property("appTheme").toString() : QString();

    bool isDarkTheme = false;
    if (theme == "dark") {
        isDarkTheme = true;
    } else if (theme == "light") {
        isDarkTheme = false;
    } else {
        const QPalette pal = topLevel ? topLevel->palette() : palette();
        isDarkTheme = pal.color(QPalette::Window).lightness() < 128;
    }

    const QString textColor = isDarkTheme ? "#FFFFFF" : "#000000";
    const QString titleColor = textColor;
    const QString linkColor = isDarkTheme ? "#FF9F1A" : "#0369A1";
    const QString dividerColor = isDarkTheme ? "rgba(255,255,255,0.28)" : "rgba(0,0,0,0.24)";

    titleLabel->setStyleSheet(QString("font-size: 30px; font-weight: 700; color: %1;").arg(titleColor));
    textBrowser->setStyleSheet(QString("QTextBrowser { background: transparent; border: none; color: %1; font-size: 20px; }").arg(textColor));

    const QString htmlContent = QString(R"(
        <div style="font-size:20px; line-height:1.8; color:%1; text-align:left;">
            <h2 style="font-size:20px; color:%2; margin:8px 0 10px 0;">🚀 快速开始</h2>
            <p style="font-size:20px; color:%1; margin:0 0 10px 0;">首次使用建议先完成下面两步配置，成功后即可开始批量生成。</p>
            <hr style="border:none; border-top:1px solid %5; margin:16px 0;"/>

            <h3 style="font-size:20px; color:%2; margin:8px 0 10px 0;">✅ 必做两步</h3>
            <ol style="margin-left:20px; color:%1; font-size:20px;">
                <li style="margin-bottom:12px; color:%1; font-size:20px;">
                    配置 AI 模型密钥：点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/E9YedBOaGoaqS0xvx3qcTrG3n4e?from=from_copylink" style="color:%3; font-size:22px; font-weight:700; text-decoration:none;">这里</a>
                </li>
                <li style="margin-bottom:12px; color:%1; font-size:20px;">
                    配置临时图床密钥：点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/WBlkdfh30otuWTxvhhucazUpnyf?from=from_copylink" style="color:%3; font-size:22px; font-weight:700; text-decoration:none;">这里</a>
                </li>
            </ol>
            <hr style="border:none; border-top:1px solid %5; margin:16px 0;"/>

            <h3 style="font-size:20px; color:%2; margin:8px 0 10px 0;">💬 常见支持</h3>
            <p style="font-size:20px; color:%1; margin:0 0 10px 0;">遇到问题可添加微信 <strong style="color:%1; font-weight:700;">312088415</strong>，拉你进群一起交流学习。</p>
            <hr style="border:none; border-top:1px solid %5; margin:16px 0;"/>

            <h3 style="font-size:20px; color:%2; margin:8px 0 10px 0;">📚 更多教程</h3>
            <p style="font-size:20px; color:%1; margin:0;">完整教程集合：点击<a href="https://g1hzbw0p4dd.feishu.cn/docx/TBeZdghZhoG9yzxju06cTxFBntg?from=from_copylink" style="color:%3; font-size:22px; font-weight:700; text-decoration:none;">这里</a></p>
        </div>
    )").arg(textColor, titleColor, linkColor, linkHoverColor, dividerColor);

    textBrowser->setHtml(htmlContent);
}
