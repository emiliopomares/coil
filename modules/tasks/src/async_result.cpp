#include <coil/foundation/error.hpp>
#include <coil/tasks/async_result.hpp>

namespace coil::tasks::detail {

Error abandonedError() {
    return Error{ErrorCode::Internal, "promise abandoned"};
}

} // namespace coil::tasks::detail
