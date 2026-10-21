#pragma once
#include <stdint.h>

namespace casting { namespace timing {
constexpr uint32_t IdleGraceMs=10000;
// A network task can publish a timestamp newer than the loop's clock snapshot.
// Clamp that small negative age instead of treating unsigned wrap as an outage.
// Real elapsed intervals here are bounded by the ten-second idle timeout.
constexpr uint32_t age(uint32_t now,uint32_t receivedAt) {
    return int32_t(now-receivedAt)<0?0:uint32_t(now-receivedAt);
}
constexpr bool idle(uint32_t now,uint32_t receivedAt) {
    return age(now,receivedAt)>=IdleGraceMs;
}
static_assert(age(1000,1001)==0,"A fresh asynchronous frame must not time out");
static_assert(!idle(1000,1001),"Clock snapshot race must preserve the video");
static_assert(!idle(9000,1000),"Short interruptions must preserve the last frame");
static_assert(idle(11000,1000),"An abandoned stream must eventually return to waiting");
static_assert(age(3,0xFFFFFFFEu)==5,"Elapsed time must work across millis rollover");
static_assert(age(0xFFFFFFFEu,3)==0,"Fresh frames across rollover must not time out");
} }
