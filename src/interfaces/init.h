#ifndef BITCOIN_INTERFACES_INIT_H
#define BITCOIN_INTERFACES_INIT_H

#include <memory>

namespace interfaces {
class Echo;

class Init
{
public:
    virtual ~Init() = default;
    virtual std::unique_ptr<Echo> makeEcho() { return nullptr; }
};
} // namespace interfaces

#endif
