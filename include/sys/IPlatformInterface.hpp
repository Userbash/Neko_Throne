#ifndef IPLATFORMINTERFACE_HPP
#define IPLATFORMINTERFACE_HPP

#include <QString>
#include <QStringList>

namespace sys {

class IPlatformInterface {
public:
    virtual ~IPlatformInterface() = default;

    virtual void addRoute(const QString &address, const QString &mask, const QString &gateway, int metric, int interfaceIndex) = 0;
    virtual void deleteRoute(const QString &address, const QString &mask, const QString &gateway, int interfaceIndex) = 0;
    virtual bool isTunInterfaceUp(const QString &name) = 0;
    virtual void blockIPv6Leaks() = 0;
    virtual void restoreIPv6() = 0;
};

} // namespace sys

#endif // IPLATFORMINTERFACE_HPP
