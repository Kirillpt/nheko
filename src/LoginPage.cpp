/*
 * nheko Copyright (C) 2017  Konstantinos Sideris <siderisk@auth.gr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QDesktopServices>
#include <QFontMetrics>
#include <QLabel>
#include <QPainter>
#include <QStyleOption>
#include <QtMath>

#include <mtx/identifiers.hpp>
#include <mtx/requests.hpp>
#include <mtx/responses/login.hpp>

#include "Config.h"
#include "Logging.h"
#include "LoginPage.h"
#include "MatrixClient.h"
#include "SSOHandler.h"
#include "ui/FlatButton.h"
#include "ui/LoadingIndicator.h"
#include "ui/OverlayModal.h"
#include "ui/RaisedButton.h"
#include "ui/TextField.h"

Q_DECLARE_METATYPE(LoginPage::LoginMethod)

using namespace mtx::identifiers;

LoginPage::LoginPage(QWidget *parent)
  : QWidget(parent)
  , inferredServerAddress_()
{
        qRegisterMetaType<LoginPage::LoginMethod>("LoginPage::LoginMethod");

        top_layout_ = new QVBoxLayout();

        top_bar_layout_ = new QHBoxLayout();
        top_bar_layout_->setSpacing(0);
        top_bar_layout_->setMargin(0);

        back_button_ = new FlatButton(this);
        back_button_->setMinimumSize(QSize(30, 30));

        top_bar_layout_->addWidget(back_button_, 0, Qt::AlignLeft | Qt::AlignVCenter);
        top_bar_layout_->addStretch(1);

        QIcon icon;
        icon.addFile(":/icons/icons/ui/angle-pointing-to-left.png");

        back_button_->setIcon(icon);
        back_button_->setIconSize(QSize(32, 32));

        QIcon logo;
        logo.addFile(":/logos/login.png");

        logo_ = new QLabel(this);
        logo_->setPixmap(logo.pixmap(128));

        logo_layout_ = new QHBoxLayout();
        logo_layout_->setContentsMargins(0, 0, 0, 20);
        logo_layout_->addWidget(logo_, 0, Qt::AlignHCenter);

        form_wrapper_ = new QHBoxLayout();
        form_widget_  = new QWidget();
        form_widget_->setMinimumSize(QSize(350, 200));

        form_layout_ = new QVBoxLayout();
        form_layout_->setSpacing(20);
        form_layout_->setContentsMargins(0, 0, 0, 30);
        form_widget_->setLayout(form_layout_);

        form_wrapper_->addStretch(1);
        form_wrapper_->addWidget(form_widget_);
        form_wrapper_->addStretch(1);

        matrixid_input_ = new TextField(this);
        matrixid_input_->setLabel(tr("Matrix ID"));
        matrixid_input_->setRegexp("@.+?:.{3,}");
        matrixid_input_->setPlaceholderText(tr("e.g @joe:matrix.org"));
        matrixid_input_->setToolTip(
          tr("Your login name. A mxid should start with @ followed by the user id. After the user "
             "id you need to include your server name after a :.\nYou can also put your homeserver "
             "address there, if your server doesn't support .well-known lookup.\nExample: "
             "@user:server.my\nIf Nheko fails to discover your homeserver, it will show you a "
             "field to enter the server manually."));

        spinner_ = new LoadingIndicator(this);
        spinner_->setFixedHeight(40);
        spinner_->setFixedWidth(40);
        spinner_->hide();

        errorIcon_ = new QLabel(this);
        errorIcon_->setPixmap(QPixmap(":/icons/icons/error.png"));
        errorIcon_->hide();

        matrixidLayout_ = new QHBoxLayout();
        matrixidLayout_->addWidget(matrixid_input_, 0, Qt::AlignVCenter);

        QFont font;

        error_matrixid_label_ = new QLabel(this);
        error_matrixid_label_->setFont(font);
        error_matrixid_label_->setWordWrap(true);

        password_input_ = new TextField(this);
        password_input_->setLabel(tr("Password"));
        password_input_->setEchoMode(QLineEdit::Password);
        password_input_->setToolTip("Your password.");

        deviceName_ = new TextField(this);
        deviceName_->setLabel(tr("Device name"));
        deviceName_->setToolTip(
          tr("A name for this device, which will be shown to others, when verifying your devices. "
             "If none is provided a default is used."));

        serverInput_ = new TextField(this);
        serverInput_->setLabel("Homeserver address");
        serverInput_->setPlaceholderText("matrix.org");
        serverInput_->setToolTip(tr("The address that can be used to contact you homeservers "
                                    "client API.\nExample: https://server.my:8787"));
        serverInput_->hide();

        serverLayout_ = new QHBoxLayout();
        serverLayout_->addWidget(serverInput_, 0, Qt::AlignVCenter);

        form_layout_->addLayout(matrixidLayout_);
        form_layout_->addWidget(error_matrixid_label_, 0, Qt::AlignHCenter);
        form_layout_->addWidget(password_input_);
        form_layout_->addWidget(deviceName_, Qt::AlignHCenter);
        form_layout_->addLayout(serverLayout_);

        error_matrixid_label_->hide();

        button_layout_ = new QHBoxLayout();
        button_layout_->setSpacing(0);
        button_layout_->setContentsMargins(0, 0, 0, 30);

        login_button_ = new RaisedButton(tr("LOGIN"), this);
        login_button_->setMinimumSize(350, 65);
        login_button_->setFontSize(20);
        login_button_->setCornerRadius(3);

        button_layout_->addStretch(1);
        button_layout_->addWidget(login_button_);
        button_layout_->addStretch(1);

        error_label_ = new QLabel(this);
        error_label_->setFont(font);
        error_label_->setWordWrap(true);

        top_layout_->addLayout(top_bar_layout_);
        top_layout_->addStretch(1);
        top_layout_->addLayout(logo_layout_);
        top_layout_->addLayout(form_wrapper_);
        top_layout_->addStretch(1);
        top_layout_->addLayout(button_layout_);
        top_layout_->addWidget(error_label_, 0, Qt::AlignHCenter);
        top_layout_->addStretch(1);

        setLayout(top_layout_);

        connect(this, &LoginPage::versionOkCb, this, &LoginPage::versionOk);
        connect(this, &LoginPage::versionErrorCb, this, &LoginPage::versionError);
        // unused thing, but if enable, you should check loginError function now,
        // because loginError now accept QLabel *label
        // connect(this, &LoginPage::loginErrorCb, this, &LoginPage::loginError);

        connect(back_button_, SIGNAL(clicked()), this, SLOT(onBackButtonClicked()));
        connect(login_button_, SIGNAL(clicked()), this, SLOT(onLoginButtonClicked()));
        connect(matrixid_input_, SIGNAL(returnPressed()), login_button_, SLOT(click()));
        connect(password_input_, SIGNAL(returnPressed()), login_button_, SLOT(click()));
        connect(deviceName_, SIGNAL(returnPressed()), login_button_, SLOT(click()));
        connect(serverInput_, SIGNAL(returnPressed()), login_button_, SLOT(click()));
        connect(matrixid_input_, SIGNAL(editingFinished()), this, SLOT(onMatrixIdEntered()));
        connect(serverInput_, SIGNAL(editingFinished()), this, SLOT(onServerAddressEntered()));
}
void
LoginPage::showError(const QString &msg)
{
        auto rect  = QFontMetrics(font()).boundingRect(msg);
        int width  = rect.width();
        int height = rect.height();
        error_label_->setFixedHeight((int)qCeil(width / 200.0) * height);
        error_label_->setText(msg);
}

void
LoginPage::showError(QLabel *label, const QString &msg)
{
        auto rect  = QFontMetrics(font()).boundingRect(msg);
        int width  = rect.width();
        int height = rect.height();
        label->setFixedHeight((int)qCeil(width / 200.0) * height);
        label->setText(msg);
}

void
LoginPage::onMatrixIdEntered()
{
        error_label_->setText("");

        User user;

        if (!matrixid_input_->isValid()) {
                error_matrixid_label_->show();
                showError(error_matrixid_label_,
                          "You have entered an invalid Matrix ID  e.g @joe:matrix.org");
                return;
        } else {
                error_matrixid_label_->setText("");
                error_matrixid_label_->hide();
        }

        try {
                user = parse<User>(matrixid_input_->text().toStdString());
        } catch (const std::exception &e) {
                showError(error_matrixid_label_,
                          "You have entered an invalid Matrix ID  e.g @joe:matrix.org");
                return;
        }

        QString homeServer = QString::fromStdString(user.hostname());
        if (homeServer != inferredServerAddress_) {
                serverInput_->hide();
                serverLayout_->removeWidget(errorIcon_);
                errorIcon_->hide();
                if (serverInput_->isVisible()) {
                        matrixidLayout_->removeWidget(spinner_);
                        serverLayout_->addWidget(spinner_, 0, Qt::AlignVCenter | Qt::AlignRight);
                        spinner_->start();
                } else {
                        serverLayout_->removeWidget(spinner_);
                        matrixidLayout_->addWidget(spinner_, 0, Qt::AlignVCenter | Qt::AlignRight);
                        spinner_->start();
                }

                inferredServerAddress_ = homeServer;
                serverInput_->setText(homeServer);

                http::client()->set_server(user.hostname());
                http::client()->well_known([this](const mtx::responses::WellKnown &res,
                                                  mtx::http::RequestErr err) {
                        if (err) {
                                using namespace boost::beast::http;

                                if (err->status_code == status::not_found) {
                                        nhlog::net()->info("Autodiscovery: No .well-known.");
                                        checkHomeserverVersion();
                                        return;
                                }

                                if (!err->parse_error.empty()) {
                                        emit versionErrorCb(
                                          tr("Autodiscovery failed. Received malformed response."));
                                        nhlog::net()->error(
                                          "Autodiscovery failed. Received malformed response.");
                                        return;
                                }

                                emit versionErrorCb(tr("Autodiscovery failed. Unknown error when "
                                                       "requesting .well-known."));
                                nhlog::net()->error("Autodiscovery failed. Unknown error when "
                                                    "requesting .well-known. {}",
                                                    err->error_code.message());
                                return;
                        }

                        nhlog::net()->info("Autodiscovery: Discovered '" + res.homeserver.base_url +
                                           "'");
                        http::client()->set_server(res.homeserver.base_url);
                        checkHomeserverVersion();
                });
        }
}

void
LoginPage::checkHomeserverVersion()
{
        http::client()->versions(
          [this](const mtx::responses::Versions &, mtx::http::RequestErr err) {
                  if (err) {
                          using namespace boost::beast::http;

                          if (err->status_code == status::not_found) {
                                  emit versionErrorCb(tr("The required endpoints were not found. "
                                                         "Possibly not a Matrix server."));
                                  return;
                          }

                          if (!err->parse_error.empty()) {
                                  emit versionErrorCb(tr("Received malformed response. Make sure "
                                                         "the homeserver domain is valid."));
                                  return;
                          }

                          emit versionErrorCb(tr(
                            "An unknown error occured. Make sure the homeserver domain is valid."));
                          return;
                  }

                  http::client()->get_login(
                    [this](mtx::responses::LoginFlows flows, mtx::http::RequestErr err) {
                            if (err || flows.flows.empty())
                                    emit versionOkCb(LoginMethod::Password);

                            if (flows.flows[0].type == mtx::user_interactive::auth_types::sso)
                                    emit versionOkCb(LoginMethod::SSO);
                            else
                                    emit versionOkCb(LoginMethod::Password);
                    });
          });
}

void
LoginPage::onServerAddressEntered()
{
        error_label_->setText("");
        http::client()->set_server(serverInput_->text().toStdString());
        checkHomeserverVersion();

        serverLayout_->removeWidget(errorIcon_);
        errorIcon_->hide();
        serverLayout_->addWidget(spinner_, 0, Qt::AlignVCenter | Qt::AlignRight);
        spinner_->start();
}

void
LoginPage::versionError(const QString &error)
{
        showError(error_label_, error);
        serverInput_->show();

        spinner_->stop();
        serverLayout_->removeWidget(spinner_);
        serverLayout_->addWidget(errorIcon_, 0, Qt::AlignVCenter | Qt::AlignRight);
        errorIcon_->show();
        matrixidLayout_->removeWidget(spinner_);
}

void
LoginPage::versionOk(LoginMethod loginMethod_)
{
        this->loginMethod = loginMethod_;

        serverLayout_->removeWidget(spinner_);
        matrixidLayout_->removeWidget(spinner_);
        spinner_->stop();

        if (loginMethod == LoginMethod::SSO) {
                password_input_->hide();
                login_button_->setText(tr("SSO LOGIN"));
        } else {
                password_input_->show();
                login_button_->setText(tr("LOGIN"));
        }

        if (serverInput_->isVisible())
                serverInput_->hide();
}

void
LoginPage::onLoginButtonClicked()
{
        error_label_->setText("");

        User user;

        if (!matrixid_input_->isValid()) {
                error_matrixid_label_->show();
                showError(error_matrixid_label_,
                          "You have entered an invalid Matrix ID  e.g @joe:matrix.org");
                return;
        } else {
                error_matrixid_label_->setText("");
                error_matrixid_label_->hide();
        }

        try {
                user = parse<User>(matrixid_input_->text().toStdString());
        } catch (const std::exception &e) {
                showError(error_matrixid_label_,
                          "You have entered an invalid Matrix ID  e.g @joe:matrix.org");
                return;
        }

        if (loginMethod == LoginMethod::Password) {
                if (password_input_->text().isEmpty())
                        return showError(error_label_, tr("Empty password"));

                http::client()->login(
                  user.localpart(),
                  password_input_->text().toStdString(),
                  deviceName_->text().trimmed().isEmpty() ? initialDeviceName()
                                                          : deviceName_->text().toStdString(),
                  [this](const mtx::responses::Login &res, mtx::http::RequestErr err) {
                          if (err) {
                                  emit showError(error_label_,
                                                 QString::fromStdString(err->matrix_error.error));
                                  emit errorOccurred();
                                  return;
                          }

                          if (res.well_known) {
                                  http::client()->set_server(res.well_known->homeserver.base_url);
                                  nhlog::net()->info("Login requested to user server: " +
                                                     res.well_known->homeserver.base_url);
                          }

                          emit loginOk(res);
                  });
        } else {
                auto sso = new SSOHandler();
                connect(sso, &SSOHandler::ssoSuccess, this, [this, sso](std::string token) {
                        mtx::requests::Login req{};
                        req.token     = token;
                        req.type      = mtx::user_interactive::auth_types::token;
                        req.device_id = deviceName_->text().trimmed().isEmpty()
                                          ? initialDeviceName()
                                          : deviceName_->text().toStdString();
                        http::client()->login(
                          req, [this](const mtx::responses::Login &res, mtx::http::RequestErr err) {
                                  if (err) {
                                          emit showError(
                                            error_label_,
                                            QString::fromStdString(err->matrix_error.error));
                                          emit errorOccurred();
                                          return;
                                  }

                                  if (res.well_known) {
                                          http::client()->set_server(
                                            res.well_known->homeserver.base_url);
                                          nhlog::net()->info("Login requested to user server: " +
                                                             res.well_known->homeserver.base_url);
                                  }

                                  emit loginOk(res);
                          });
                        sso->deleteLater();
                });
                connect(sso, &SSOHandler::ssoFailed, this, [this, sso]() {
                        emit showError(error_label_, tr("SSO login failed"));
                        emit errorOccurred();
                        sso->deleteLater();
                });

                QDesktopServices::openUrl(
                  QString::fromStdString(http::client()->login_sso_redirect(sso->url())));
        }

        emit loggingIn();
}

void
LoginPage::reset()
{
        matrixid_input_->clear();
        password_input_->clear();
        password_input_->show();
        serverInput_->clear();

        spinner_->stop();
        errorIcon_->hide();
        serverLayout_->removeWidget(spinner_);
        serverLayout_->removeWidget(errorIcon_);
        matrixidLayout_->removeWidget(spinner_);

        inferredServerAddress_.clear();
}

void
LoginPage::onBackButtonClicked()
{
        emit backButtonClicked();
}

void
LoginPage::paintEvent(QPaintEvent *)
{
        QStyleOption opt;
        opt.init(this);
        QPainter p(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
