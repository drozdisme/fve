#include "../core/types/types.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_types() {
    cur = "types";
    Dim m = Dim::base(1);
    Dim s = Dim::base(2);
    CHECK(Dim::none().dimensionless());
    CHECK(!m.dimensionless());

    Dim area = mul(m, m);
    CHECK(area.e[1] == 2);
    Dim vel = div(m, s);
    CHECK(vel.e[1] == 1 && vel.e[2] == -1);
    Dim accel = div(vel, s);
    CHECK(accel.e[2] == -2);

    Dim m3 = powd(m, 3);
    CHECK(m3.e[1] == 3);

    CHECK(compatible(m, m));
    CHECK(!compatible(m, s));
    CHECK(mul(m, Dim::none()) == m);
    CHECK(div(m, m).dimensionless());

    CHECK(m.str() == "m");
    CHECK(area.str() == "m^2");
    CHECK(Dim::none().str() == "1");

    Unit kn{"kN", div(mul(Dim::base(0), Dim::base(1)), powd(Dim::base(2), 2)), 1000.0, 0.0};
    NEAR(kn.to_si(2.0), 2000.0, 1e-9);
    NEAR(kn.from_si(2000.0), 2.0, 1e-9);

    Phys pos = Phys::pos(m);
    CHECK(pos.range.lo == 0.0);
    Phys ofr = Phys::of(Dim::none(), -1.0, 5.0);
    CHECK(ofr.range == (Ival{-1.0, 5.0}));

    Symbol t{"t", m, Ival{0.5, 2.0}, "mm"};
    CHECK(t.id == "t");
    CHECK(t.phys.contains(1.0));
}
