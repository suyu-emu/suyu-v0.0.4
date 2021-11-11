#include "common/multi_level_page_table.inc"

namespace Common {
template class Common::MultiLevelPageTable<GPUVAddr>;
template class Common::MultiLevelPageTable<VAddr>;
template class Common::MultiLevelPageTable<PAddr>;
template class Common::MultiLevelPageTable<u32>;
} // namespace Common
