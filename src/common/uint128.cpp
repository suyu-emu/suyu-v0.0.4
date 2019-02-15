
namespace Common {

std::pair<u64, u64> udiv128(u128 dividend, u64 divisor) {
    u64 remainder = dividend[0] % divisor;
    u64 accum = dividend[0] / divisor;
    if (dividend[1] == 0)
        return {accum, remainder};
    // We ignore dividend[1] / divisor as that overflows
    u64 first_segment = (dividend[1] % divisor) << 32;
    accum += (first_segment / divisor) << 32;
    u64 second_segment = (first_segment % divisor) << 32;
    accum += (second_segment / divisor);
    remainder += second_segment % divisor;
    return {accum, remainder};
}

} // namespace Common
