#ifndef PARAM_H
#define PARAM_H

#include <QVariant>

using namespace Qt::Literals::StringLiterals;

class Param : public QVariant
{
public:
      Param(const QString &name,
            const QString &prefix,
            const QMetaType::Type &typeId,
            const QVariant &value = QVariant())
          : QVariant(value),
            name(name),
            prefix(prefix),
            typeId(typeId){};

      ~Param(){};

      QString name;
      QString prefix;
      QMetaType::Type typeId;

      enum Key
      {
            Port,
            Address,
            Url,
            Host,
            Sources,
            Strict,
            Token,
            Endpoint,
            Cnrelay
      };
};

#endif // PARAM_H
