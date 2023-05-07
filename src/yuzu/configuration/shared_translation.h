#include <forward_list>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <utility>
#include <QString>

class QWidget;

namespace ConfigurationShared {
using TranslationMap = std::map<std::string, std::pair<QString, QString>>;

std::unique_ptr<TranslationMap> InitializeTranslations(QWidget* parent);

std::forward_list<QString> ComboboxEnumeration(std::type_index type, QWidget* parent);

} // namespace ConfigurationShared
