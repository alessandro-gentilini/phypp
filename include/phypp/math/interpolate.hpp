#ifndef PHYPP_MATH_INTERPOLATE_HPP
#define PHYPP_MATH_INTERPOLATE_HPP

#include "phypp/core/vec.hpp"
#include "phypp/core/range.hpp"
#include "phypp/core/error.hpp"
#include "phypp/utility/generic.hpp"
#include "phypp/math/base.hpp"

namespace phypp {
    inline double interpolate(double y1, double y2, double x1, double x2, double x) {
        double a = (x - x1)/(x2 - x1);
        return y1 + (y2 - y1)*a;
    }

    inline std::pair<double,double> interpolate_err(double y1, double y2, double e1, double e2, double x1, double x2, double x) {
        double a = (x - x1)/(x2 - x1);
        return std::make_pair(y1 + (y2 - y1)*a, sqrt(sqr(e1*(1-a)) + sqr(e2*a)));
    }

    // Perform linear interpolation of data 'y' of position 'x' at new positions 'nx'.
    // Assumes that the arrays only contain finite elements, and that 'x' is properly sorted. If one of
    // the arrays contains special values (NaN, inf, ...), all the points that would use these values
    // will be contaminated. If 'x' is not properly sorted, the result will simply be wrong.
    template<std::size_t DI = 1, std::size_t DX = 1, typename TypeX2 = double, typename TypeY = double,
        typename TypeX1 = double>
    auto interpolate(const vec<DI,TypeY>& y, const vec<DI,TypeX1>& x, const vec<DX,TypeX2>& nx) ->
        vec<DX,decltype(y[0]*x[0])> {

        using rtypey = meta::rtype_t<TypeY>;
        using rtypex = meta::rtype_t<TypeX1>;

        phypp_check(y.size() == x.size(),
            "interpolate: 'x' and 'y' arrays must contain the same number of elements");
        phypp_check(y.size() >= 2,
            "interpolate: 'x' and 'y' arrays must contain at least 2 elements");

        uint_t nmax = x.size();
        vec<DX,decltype(y[0]*x[0])> r; r.reserve(nx.size());
        for (auto& tx : nx) {
            uint_t low = lower_bound(tx, x);

            rtypey ylow, yup;
            rtypex xlow, xup;
            if (low != npos) {
                if (low != nmax-1) {
                    ylow = y.safe[low]; yup = y.safe[low+1];
                    xlow = x.safe[low]; xup = x.safe[low+1];
                } else {
                    ylow = y.safe[low-1]; yup = y.safe[low];
                    xlow = x.safe[low-1]; xup = x.safe[low];
                }
            } else {
                ylow = y.safe[0]; yup = y.safe[1];
                xlow = x.safe[0]; xup = x.safe[1];
            }

            r.push_back(ylow + (yup - ylow)*(tx - xlow)/(xup - xlow));
        }

        return r;
    }

    // Perform linear interpolation of data 'y' of position 'x' at new position 'nx'.
    // Assumes that the arrays only contain finite elements, and that 'x' is properly sorted. If one of
    // the arrays contains special values (NaN, inf, ...), all the points that would use these values
    // will be contaminated. If 'x' is not properly sorted, the result will simply be wrong.
    template<std::size_t DI = 1, typename TypeY = double, typename TypeX = double, typename T = double,
        typename enable = typename std::enable_if<!meta::is_vec<T>::value>::type>
    auto interpolate(const vec<DI,TypeY>& y, const vec<DI,TypeX>& x, const T& nx) ->
        decltype(y[0]*x[0]) {

        using rtypey = meta::rtype_t<TypeY>;
        using rtypex = meta::rtype_t<TypeX>;

        phypp_check(n_elements(y) == n_elements(x),
            "interpolate: 'x' and 'y' arrays must contain the same number of elements");
        phypp_check(n_elements(y) >= 2,
            "interpolate: 'x' and 'y' arrays must contain at least 2 elements");

        uint_t nmax = x.size();
        uint_t low = lower_bound(nx, x);

        rtypey ylow, yup;
        rtypex xlow, xup;
        if (low != npos) {
            if (low != nmax-1) {
                ylow = y.safe[low]; yup = y.safe[low+1];
                xlow = x.safe[low]; xup = x.safe[low+1];
            } else {
                ylow = y.safe[low-1]; yup = y.safe[low];
                xlow = x.safe[low-1]; xup = x.safe[low];
            }
        } else {
            ylow = y.safe[0]; yup = y.safe[1];
            xlow = x.safe[0]; xup = x.safe[1];
        }

        return ylow + (yup - ylow)*(nx - xlow)/(xup - xlow);
    }

    // Perform linear interpolation of data 'y' of position 'x' at new positions 'nx'.
    // Assumes that the arrays only contain finite elements, and that 'x' is properly sorted. If one of
    // the arrays contains special values (NaN, inf, ...), all the points that would use these values
    // will be contaminated. If 'x' is not properly sorted, the result will simply be wrong.
    template<std::size_t DI = 1, std::size_t DX = 1, typename TypeX2 = double, typename TypeY = double,
        typename TypeE = double, typename TypeX1 = double>
    auto interpolate(const vec<DI,TypeY>& y, const vec<DI,TypeE>& e, const vec<DI,TypeX1>& x, const vec<DX,TypeX2>& nx) ->
        std::pair<vec<DX,decltype(y[0]*x[0])>,vec<DX,decltype(y[0]*x[0])>> {

        using rtypey = meta::rtype_t<TypeY>;
        using rtypee = meta::rtype_t<TypeE>;
        using rtypex = meta::rtype_t<TypeX1>;

        phypp_check(y.size() == x.size(),
            "interpolate: 'x' and 'y' arrays must contain the same number of elements");
        phypp_check(y.size() >= 2,
            "interpolate: 'x' and 'y' arrays must contain at least 2 elements");

        uint_t nmax = x.size();
        std::pair<vec<DX,decltype(y[0]*x[0])>,vec<DX,decltype(y[0]*x[0])>> p;
        p.first.reserve(nx.size());
        p.second.reserve(nx.size());
        for (auto& tx : nx) {
            uint_t low = lower_bound(tx, x);

            rtypey ylow, yup;
            rtypee elow, eup;
            rtypex xlow, xup;
            if (low != npos) {
                if (low != nmax-1) {
                    ylow = y.safe[low]; yup = y.safe[low+1];
                    elow = e.safe[low]; eup = e.safe[low+1];
                    xlow = x.safe[low]; xup = x.safe[low+1];
                } else {
                    ylow = y.safe[low-1]; yup = y.safe[low];
                    elow = e.safe[low-1]; eup = e.safe[low];
                    xlow = x.safe[low-1]; xup = x.safe[low];
                }
            } else {
                ylow = y.safe[0]; yup = y.safe[1];
                elow = e.safe[0]; eup = e.safe[1];
                xlow = x.safe[0]; xup = x.safe[1];
            }

            double a = (tx - xlow)/(xup - xlow);
            p.first.push_back(ylow + (yup - ylow)*a);
            p.second.push_back(sqrt(sqr(elow*(1-a)) + sqr(eup*a)));
        }

        return p;
    }

    inline double bilinear(double v1, double v2, double v3, double v4, double x, double y) {
        return v1*(1.0-x)*(1.0-y) + v2*(1.0-x)*y + v3*x*(1.0-y) + v4*x*y;
    }

    template<typename Type>
    meta::rtype_t<Type> bilinear(const vec<2,Type>& map, double x, double y) {
        int_t tix = floor(x);
        int_t tiy = floor(y);
        double dx = x - tix;
        double dy = y - tiy;

        if (tix < 0) {
            tix = 0;
            dx = x - tix;
        } else if (uint_t(tix) >= map.dims[0]-1) {
            tix = map.dims[0]-2;
            dx = x - tix;
        }

        if (tiy < 0) {
            tiy = 0;
            dy = y - tiy;
        } else if (uint_t(tiy) >= map.dims[1]-1) {
            tiy = map.dims[1]-2;
            dy = y - tiy;
        }

        uint_t ix = tix;
        uint_t iy = tiy;

        return bilinear(map.safe(ix,iy), map.safe(ix,iy+1),
                        map.safe(ix+1,iy), map.safe(ix+1,iy+1), dx, dy);
    }

    template<typename Type>
    meta::rtype_t<Type> bilinear(const vec<2,Type>& map, const vec1d& mx,
        const vec1d& my, double x, double y) {

        phypp_check(map.dims[0] == mx.size(), "incompatible size of MAP and MX (", map.dims,
            " vs. ", mx.size(), ")");
        phypp_check(map.dims[1] == my.size(), "incompatible size of MAP and MY (", map.dims,
            " vs. ", my.size(), ")");

        double ux = interpolate(dindgen(mx.size()), mx, x);
        double uy = interpolate(dindgen(my.size()), my, y);
        return bilinear(map, ux, uy);
    }

    template<typename Type, std::size_t D, typename TX, typename TY>
    vec<D,meta::rtype_t<Type>> bilinear(const vec<2,Type>& map, const vec1d& mx,
        const vec1d& my, const vec<D,TX>& x, const vec<D,TY>& y) {

        phypp_check(map.dims[0] == mx.size(), "incompatible size of MAP and MX (", map.dims,
            " vs. ", mx.size(), ")");
        phypp_check(map.dims[1] == my.size(), "incompatible size of MAP and MY (", map.dims,
            " vs. ", my.size(), ")");
        phypp_check(x.dims == y.dims, "incompatible dimensions for X and Y (", x.dims,
            " vs. ", y.size(), ")");

        vec<D,meta::rtype_t<Type>> v(x.dims);

        auto ux = interpolate(dindgen(mx.size()), mx, x);
        auto uy = interpolate(dindgen(my.size()), my, y);

        for (uint_t i : range(x)) {
            v.safe[i] = bilinear(map, ux.safe[i], uy.safe[i]);
        }

        return v;
    }


    template<typename Type, typename TypeD = double>
    meta::rtype_t<Type> bilinear_strict(const vec<2,Type>& map, double x, double y,
        TypeD def = 0.0) {

        int_t tix = floor(x);
        int_t tiy = floor(y);

        if (tix < 0 || uint_t(tix) >= map.dims[0]-1 || tiy < 0 || uint_t(tiy) >= map.dims[1]-1) {
            return def;
        }

        double dx = x - tix;
        double dy = y - tiy;
        uint_t ix = tix;
        uint_t iy = tiy;

        return bilinear(map.safe(ix,iy), map.safe(ix,iy+1),
                        map.safe(ix+1,iy), map.safe(ix+1,iy+1), dx, dy);
    }

    template<typename Type>
    vec<2,meta::rtype_t<Type>> rebin(const vec<2,Type>& map, const vec1d& mx,
        const vec1d& my, const vec1d& x, const vec1d& y) {

        phypp_check(map.dims[0] == mx.size(), "incompatible size of MAP and MX (", map.dims,
            " vs. ", mx.size(), ")");
        phypp_check(map.dims[1] == my.size(), "incompatible size of MAP and MY (", map.dims,
            " vs. ", my.size(), ")");

        vec<2,meta::rtype_t<Type>> v(x.size(), y.size());

        vec1d ux = interpolate(dindgen(mx.size()), mx, x);
        vec1d uy = interpolate(dindgen(my.size()), my, y);

        for (uint_t ix : range(ux))
        for (uint_t iy : range(uy)) {
            v.safe(ix,iy) = bilinear(map, ux.safe[ix], uy.safe[iy]);
        }

        return v;
    }
}

#endif
