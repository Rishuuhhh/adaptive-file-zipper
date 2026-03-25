#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "file_io.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

// codeMap serialize -> deserialize wapas same hona chahiye
RC_GTEST_PROP(FileIO, CodeMapRoundTrip, ()) {
    auto m = *rc::gen::container<std::unordered_map<std::string, std::string>>(
        rc::gen::arbitrary<std::string>(),
        rc::gen::arbitrary<std::string>()
    );
    RC_ASSERT(deserializeCodeMap(serializeCodeMap(m)) == m);
}

// pack -> unpack wapas same data dena chahiye
RC_GTEST_PROP(FileIO, PackRoundTrip, ()) {
    auto meth = *rc::gen::map(
        rc::gen::arbitrary<std::string>(),
        [](std::string s) {
            s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
            return s;
        }
    );

    auto ei = *rc::gen::inRange<int>(0, 800);
    double ev = ei / 100.0;

    auto cms = *rc::gen::arbitrary<std::string>();
    auto pay = *rc::gen::arbitrary<std::string>();

    auto pk = packData(meth, ev, cms, pay);

    std::string rm, rc2, rp;
    double re = 0.0;
    unpackData(pk, rm, re, rc2, rp);

    RC_ASSERT(rm == meth);
    RC_ASSERT(rc2 == cms);
    RC_ASSERT(rp == pay);
    RC_ASSERT(std::abs(re - ev) < 1e-6);
}
