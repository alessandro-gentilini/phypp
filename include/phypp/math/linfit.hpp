#ifndef PHYPP_MATH_LINFIT_HPP
#define PHYPP_MATH_LINFIT_HPP

#include "phypp/core/vec.hpp"
#include "phypp/core/error.hpp"
#include "phypp/core/range.hpp"
#include "phypp/math/base.hpp"
#include "phypp/math/matrix.hpp"

namespace phypp {
    struct linfit_result {
        bool success;

        double chi2;
        vec1d  params;
        vec1d  errors;
        vec2d  cov;

        // Reflection data
        MEMBERS1(success, chi2, params, errors, cov);
        MEMBERS2("linfit_result", MAKE_MEMBER(success), MAKE_MEMBER(chi2),
            MAKE_MEMBER(params), MAKE_MEMBER(errors), MAKE_MEMBER(cov));
    };

    namespace impl {
        template<typename T, typename TypeE>
        void linfit_make_cache_(vec2d& cache, const TypeE& ye, uint_t i, T&& t) {
            cache.safe(i,_) = flatten(t/ye);
        }

        template<typename T, typename TypeE, typename ... Args>
        void linfit_make_cache_(vec2d& cache, const TypeE& ye, uint_t i, T&& t, Args&& ... args) {
            cache.safe(i,_) = flatten(t/ye);
            linfit_make_cache_(cache, ye, i+1, args...);
        }

        template<typename TypeY, typename TypeE>
        linfit_result linfit_do_(const TypeY& y, const TypeE& ye, const vec2d& cache) {
            linfit_result fr;

            uint_t np = cache.dims[0];
            uint_t nm = cache.dims[1];

            // Solving 'y +/- e = sum over i of a[i]*x[i]' to get all a[i]'s
            vec2d alpha(np,np);
            vec1d beta(np);
            auto tmp = flatten(y/ye);
            for (uint_t i = 0; i < np; ++i) {
                for (uint_t j = 0; j < np; ++j) {
                    if (i <= j) {
                        alpha.safe(i,j) = 0.0;
                        // alpha(i,j) = sum over all points of x[i]*x[j]/e^2
                        for (uint_t m = 0; m < nm; ++m) {
                            alpha.safe(i,j) += cache.safe(i,m)*cache.safe(j,m);
                        }
                    } else {
                        alpha.safe(i,j) = alpha.safe(j,i);
                    }
                }

                beta.safe[i] = 0.0;
                // beta[i] = sum over all points of x[i]*y/e^2
                for (uint_t m = 0; m < nm; ++m) {
                    beta.safe[i] += cache.safe(i,m)*tmp.safe[m];
                }
            }

            if (!matrix::inplace_invert_symmetric(alpha)) {
                fr.success = false;
                fr.chi2 = dnan;
                fr.params = replicate(dnan, np);
                fr.errors = replicate(dnan, np);
                matrix::symmetrize(alpha);
                fr.cov = alpha;
                return fr;
            }

            matrix::symmetrize(alpha);
            fr.success = true;
            fr.params = matrix::product(alpha, beta);
            fr.errors = sqrt(matrix::diagonal(alpha));
            fr.cov = alpha;

            vec1d model(nm);
            for (uint_t m = 0; m < nm; ++m) {
                model.safe[m] = 0.0;
                for (uint_t i = 0; i < np; ++i) {
                    model.safe[m] += fr.params.safe[i]*cache.safe(i,m);
                }
            }

            fr.chi2 = total(sqr(model - tmp));

            return fr;
        }

        template<typename TY>
        void linfit_error_dims_(const TY& y, uint_t i) {}

        template<typename TY, typename U, typename ... Args>
        void linfit_error_dims_(const TY& y, uint_t i, const U& t, const Args& ... args) {
            phypp_check(same_dims_or_scalar(y, t), "incompatible dimensions between Y and X",
                i, " (", dim(y), " vs. ", dim(t), ")");
            linfit_error_dims_(y, i+1, args...);
        }
    }

    template<typename TypeY, typename TypeE, typename ... Args>
    linfit_result linfit(const TypeY& y, const TypeE& ye, Args&&... args) {
        bool bad = !same_dims_or_scalar(y, ye, args...);
        if (bad) {
            phypp_check(same_dims_or_scalar(y, ye), "incompatible dimensions between Y and "
                "YE arrays (", dim(y), " vs. ", dim(ye), ")");
            impl::linfit_error_dims_(y, 0, args...);
        }

        uint_t np = sizeof...(Args);
        uint_t nm = n_elements(y);

        vec2d cache(np,nm);
        impl::linfit_make_cache_(cache, ye, 0, std::forward<Args>(args)...);

        return impl::linfit_do_(y, ye, cache);
    }

    template<std::size_t Dim, typename TypeY, typename TypeE, typename TypeX>
    linfit_result linfit_pack(const vec<Dim,TypeY>& y, const vec<Dim,TypeE>& ye,
        const vec<Dim+1,TypeX>& x) {
        bool good = true;
        for (uint_t i : range(Dim)) {
            if (x.dims[i+1] != ye.dims[i] || x.dims[i+1] != y.dims[i]) {
                good = false;
                break;
            }
        }

        phypp_check(good, "incompatible dimensions between X, Y and YE arrays (", x.dims,
            " vs. ", y.dims, " vs. ", ye.dims, ")")

        linfit_result fr;

        uint_t np = x.dims[0];
        uint_t nm = y.size();

        vec2d cache(np,nm);
        for (uint_t i = 0; i < np; ++i) {
            for (uint_t j = 0; j < nm; ++j) {
                cache.safe(i,j) = x.safe[i*x.pitch(0) + j]/ye.safe[j];
            }
        }

        return impl::linfit_do_(y, ye, cache);
    }

    template<typename TypeE>
    struct linfit_batch_t {
        TypeE ye;
        vec2d cache;
        vec1d beta;
        vec2d alpha;
        linfit_result fr;

        template<typename ... Args>
        linfit_batch_t(const TypeE& e, Args&&... args) : ye(e) {
            uint_t np = sizeof...(Args);
            uint_t nm = n_elements(ye);

            cache.resize(np,nm);
            beta.resize(np);
            alpha.resize(np,np);
            impl::linfit_make_cache_(cache, ye, 0, std::forward<Args>(args)...);

            // Solving 'y +/- e = sum over i of a[i]*x[i]' to get all a[i]'s
            for (uint_t i = 0; i < np; ++i)
            for (uint_t j = 0; j < np; ++j) {
                if (i <= j) {
                    alpha.safe(i,j) = 0.0;
                    // alpha(i,j) = sum over all points of x[i]*x[j]/e^2
                    for (uint_t m = 0; m < nm; ++m) {
                        alpha.safe(i,j) += cache.safe(i,m)*cache.safe(j,m);
                    }
                } else {
                    alpha.safe(i,j) = alpha.safe(j,i);
                }
            }

            if (!matrix::inplace_invert_symmetric(alpha)) {
                fr.success = false;
                fr.chi2 = dnan;
                fr.params = replicate(dnan, np);
                fr.errors = replicate(dnan, np);
            } else {
                fr.success = true;
                fr.errors = sqrt(matrix::diagonal(alpha));
            }

            matrix::symmetrize(alpha);
            fr.cov = alpha;
        }

        template<typename TypeY>
        void fit(const TypeY& y) {
            phypp_check(same_dims_or_scalar(y, ye), "incompatible dimensions between Y and "
                "YE arrays (", dim(y), " vs. ", dim(ye), ")");

            if (!fr.success) return;

            uint_t np = cache.dims[0];
            uint_t nm = cache.dims[1];

            auto tmp = flatten(y/ye);
            for (uint_t i = 0; i < np; ++i) {
                beta.safe[i] = 0.0;
                // beta[i] = sum over all points of x[i]*y/e^2
                for (uint_t m = 0; m < nm; ++m) {
                    beta.safe[i] += cache.safe(i,m)*tmp.safe[m];
                }
            }

            fr.params = matrix::product(alpha, beta);

            vec1d model(nm);
            for (uint_t m = 0; m < nm; ++m) {
                model.safe[m] = 0.0;
                for (uint_t i = 0; i < np; ++i) {
                    model.safe[m] += fr.params.safe[i]*cache.safe(i,m);
                }
            }

            fr.chi2 = total(sqr(model - tmp));
        }
    };

    template<typename TypeE, typename ... Args>
    linfit_batch_t<TypeE> linfit_batch(const TypeE& e, Args&&... args) {
        return linfit_batch_t<TypeE>(e, std::forward<Args>(args)...);
    }
}

#endif
