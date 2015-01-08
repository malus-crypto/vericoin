#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "util.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "cookiejar.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "bitcoingui.h"
#include "webview.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QDesktopServices>
#include <QUrl>
#include <QWebView>

#define DECORATION_SIZE 46
#define NUM_ITEMS 3

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::VRC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = COLOR_BAREADDRESS;
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if (amount > 0)
        {
            foreground = COLOR_POSITIVE;
        }
        else
        {
            foreground = COLOR_BAREADDRESS;
        }
        painter->setPen(foreground);
        QString amountText =  BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    ui->setupUi(this);

    QUrl statsUrl(QString(walletUrl).append("wallet/stats.html?v=1"));
    CookieJar *statsJar = new CookieJar;
    ui->stats->page()->networkAccessManager()->setCookieJar(statsJar);
    ui->stats->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect(ui->stats->page(), SIGNAL(linkClicked(QUrl)), this, SLOT(myOpenUrl(QUrl)));
    connect(ui->stats->page()->networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(sslErrorHandler(QNetworkReply*, const QList<QSslError> & )));
    ui->stats->load(statsUrl);

    QUrl valueUrl(QString(walletUrl).append("wallet/chart.html?v=1"));
    CookieJar *valueJar = new CookieJar;
    ui->value->page()->networkAccessManager()->setCookieJar(valueJar);
    ui->value->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect(ui->value->page(), SIGNAL(linkClicked(QUrl)), this, SLOT(myOpenUrl(QUrl)));
    connect(ui->value->page()->networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(sslErrorHandler(QNetworkReply*, const QList<QSslError> & )));
    ui->value->load(valueUrl);

    QUrl tickerUrl(QString(walletUrl).append("wallet/ticker.html?v=1"));
    CookieJar *tickerJar = new CookieJar;
    ui->ticker->page()->networkAccessManager()->setCookieJar(tickerJar);
    ui->ticker->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    ui->ticker->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect(ui->ticker->page(), SIGNAL(linkClicked(QUrl)), this, SLOT(myOpenUrl(QUrl)));
    connect(ui->ticker->page()->networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(sslErrorHandler(QNetworkReply*, const QList<QSslError> & )));
    ui->ticker->load(tickerUrl);

    // Recent transactionsBalances
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 1));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listTransactions->setMouseTracking(true);
    ui->listTransactions->viewport()->setAttribute(Qt::WA_Hover, true);
    ui->listTransactions->setStyleSheet("QListView { color: " + STRING_VERIFONT + "; } QListView::hover { background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #fafbfe, stop: 1 #ECF3FA); }");
    ui->listTransactions->setFont(veriFont);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    delete ui->stats->page()->networkAccessManager()->cookieJar();
    delete ui->value->page()->networkAccessManager()->cookieJar();

    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    BitcoinUnits *bcu = new BitcoinUnits(this, this->model);
    int unit = model->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(bcu->formatWithUnit(unit, balance));
    ui->labelStake->setText(bcu->formatWithUnit(unit, stake));
    ui->labelUnconfirmed->setText(bcu->formatWithUnit(unit, unconfirmedBalance));
    //ui->labelImmature->setText(bcu->formatWithUnit(unit, immatureBalance));
    ui->labelTotal->setText(bcu->formatWithUnit(unit, balance + stake + unconfirmedBalance + immatureBalance));
    // ui->labelImmature->setVisible(true);
    //ui->labelImmatureText->setVisible(true);
    delete bcu;
}

void OverviewPage::setNumTransactions(int count)
{
    //ui->labelNumTransactions->setText(QLocale::system().toString(count));
}

void OverviewPage::setModel(WalletModel *model)
{
    this->model = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        connect(model->getOptionsModel(), SIGNAL(decimalPointsChanged(int)), this, SLOT(updateDecimalPoints()));
    }

    // update the display unit, to not use the default ("VRC")
    updateDisplayUnit();
    // update the decimal points
    updateDecimalPoints();
}

void OverviewPage::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = model->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateDecimalPoints()
{
    if(model && model->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);
    }
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::myOpenUrl(QUrl url)
{
    QDesktopServices::openUrl(url);
}

void OverviewPage::sslErrorHandler(QNetworkReply* qnr, const QList<QSslError> & errlist)
{
    qnr->ignoreSslErrors();
}
