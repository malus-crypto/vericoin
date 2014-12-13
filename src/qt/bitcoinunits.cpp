#include "bitcoinunits.h"
#include "optionsmodel.h"
#include <QStringList>

WalletModel *walletModel;

BitcoinUnits::BitcoinUnits(QObject *parent, WalletModel *wModel):
        QAbstractListModel(parent),
        unitlist(availableUnits())
{
    walletModel = wModel;
}

QList<BitcoinUnits::Unit> BitcoinUnits::availableUnits()
{
    QList<BitcoinUnits::Unit> unitlist;
    unitlist.append(VRC);
    unitlist.append(mVRC);
    unitlist.append(uVRC);
    return unitlist;
}

bool BitcoinUnits::valid(int unit)
{
    switch(unit)
    {
    case VRC:
    case mVRC:
    case uVRC:
        return true;
    default:
        return false;
    }
}

QString BitcoinUnits::name(int unit)
{
    switch(unit)
    {
    case VRC: return QString("VRC");
    case mVRC: return QString("mVRC");
    case uVRC: return QString::fromUtf8("μVRC");
    default: return QString("???");
    }
}

QString BitcoinUnits::description(int unit)
{
    switch(unit)
    {
    case VRC: return QString("VeriCoins");
    case mVRC: return QString("Milli-VeriCoins (1 / 1,000)");
    case uVRC: return QString("Micro-VeriCoins (1 / 1,000,000)");
    default: return QString("???");
    }
}

qint64 BitcoinUnits::factor(int unit)
{
    switch(unit)
    {
    case VRC:  return 100000000;
    case mVRC: return 100000;
    case uVRC: return 100;
    default:   return 100000000;
    }
}

int BitcoinUnits::amountDigits(int unit)
{
    switch(unit)
    {
    case VRC: return 8; // 21,000,000 (# digits, without commas)
    case mVRC: return 11; // 21,000,000,000
    case uVRC: return 14; // 21,000,000,000,000
    default: return 0;
    }
}

int BitcoinUnits::maxdecimals(int unit)
{
    switch(unit)
    {
        case VRC: return 8;
        case mVRC: return 5;
        case uVRC: return 2;
        default: return 0;
    }
}

int BitcoinUnits::decimals(int unit)
{
    if(walletModel && walletModel->getOptionsModel())
    {
        return walletModel->getOptionsModel()->getDecimalPoints();
    }
    else
    {
        return maxdecimals(unit);
    }
}

QString BitcoinUnits::format(int unit, qint64 n, bool fPlus)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    if(!valid(unit))
        return QString(); // Refuse to format invalid unit
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    qint64 remainder = n_abs % coin;
    QString quotient_str = QString::number(quotient);
    QString remainder_str = QString::number(remainder).rightJustified(maxdecimals(unit), '0').left(num_decimals);

    // Right-trim excess zeros after the decimal point
    //int nTrim = 0;
    //for (int i = remainder_str.size()-1; i>=2 && (remainder_str.at(i) == '0'); --i)
    //    ++nTrim;
    //remainder_str.chop(nTrim);

    // Pad zeros after remainder up to number of decimals
    for (int i = remainder_str.size(); i < num_decimals; ++i)
        remainder_str.append("0");

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fPlus && n > 0)
        quotient_str.insert(0, '+');

    if (remainder_str.size())
        return quotient_str + QString(".") + remainder_str;
    else
        return quotient_str;
}

QString BitcoinUnits::formatWithUnit(int unit, qint64 amount, bool plussign)
{
    return format(unit, amount, plussign) + QString(" ") + name(unit);
}

bool BitcoinUnits::parse(int unit, const QString &value, qint64 *val_out)
{
    if(!valid(unit) || value.isEmpty())
        return false; // Refuse to parse invalid unit or empty string
    int num_decimals = maxdecimals(unit);
    QStringList parts = value.split(".");

    if(parts.size() > 2)
    {
        return false; // More than one dot
    }
    QString whole = parts[0];
    QString decimals;

    if(parts.size() > 1)
    {
        decimals = parts[1];
    }
    if(decimals.size() > num_decimals)
    {
        return false; // Exceeds max precision
    }
    bool ok = false;
    QString str = whole + decimals.leftJustified(num_decimals, '0');

    if(str.size() > 18)
    {
        return false; // Longer numbers will exceed 63 bits
    }
    qint64 retvalue = str.toLongLong(&ok);
    if(val_out)
    {
        *val_out = retvalue;
    }
    return ok;
}

int BitcoinUnits::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return unitlist.size();
}

QVariant BitcoinUnits::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if(row >= 0 && row < unitlist.size())
    {
        Unit unit = unitlist.at(row);
        switch(role)
        {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return QVariant(name(unit));
        case Qt::ToolTipRole:
            return QVariant(description(unit));
        case UnitRole:
            return QVariant(static_cast<int>(unit));
        }
    }
    return QVariant();
}
