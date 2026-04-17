#ifndef BITCOIN_INTERFACES_ECHO_H
#define BITCOIN_INTERFACES_ECHO_H

#include <memory>
#include <string>

namespace interfaces {
class Echo
{
public:
    virtual ~Echo() = default;
    virtual std::string echo(const std::string& echo) = 0;
};

std::unique_ptr<Echo> MakeEcho();
} // namespace interfaces

#endif
