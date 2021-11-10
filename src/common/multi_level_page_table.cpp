#include "common/multi_level_page_table.inc"

namespace Common {
template class Common::MultiLevelPageTable<GPUVAddr>;
template class Common::MultiLevelPageTable<VAddr>;
template class Common::MultiLevelPageTable<PAddr>;
} // namespace Common
