/**
 * SmartHub Protocol Factory Implementation
 */

#include <smarthub/protocols/ProtocolFactory.hpp>

namespace smarthub {

ProtocolFactory& ProtocolFactory::instance() {
    static ProtocolFactory instance;
    return instance;
}

void ProtocolFactory::registerProtocol(const std::string& name, CreatorFunc creator,
                                       const ProtocolInfo& info) {
    Registration reg;
    reg.creator = std::move(creator);
    reg.info = info;
    if (reg.info.name.empty()) {
        reg.info.name = name;
    }
    m_protocols[name] = std::move(reg);
}

void ProtocolFactory::unregisterProtocol(const std::string& name) {
    m_protocols.erase(name);
}

bool ProtocolFactory::hasProtocol(const std::string& name) const {
    return m_protocols.find(name) != m_protocols.end();
}

ProtocolHandlerPtr ProtocolFactory::create(const std::string& name, EventBus& eventBus,
                                           const nlohmann::json& config) {
    auto it = m_protocols.find(name);
    if (it == m_protocols.end()) {
        return nullptr;
    }
    return it->second.creator(eventBus, config);
}

std::vector<std::string> ProtocolFactory::availableProtocols() const {
    std::vector<std::string> names;
    names.reserve(m_protocols.size());
    for (const auto& [name, reg] : m_protocols) {
        names.push_back(name);
    }
    return names;
}

ProtocolFactory::ProtocolInfo ProtocolFactory::getProtocolInfo(const std::string& name) const {
    auto it = m_protocols.find(name);
    if (it != m_protocols.end()) {
        return it->second.info;
    }
    return {};
}

} // namespace smarthub
