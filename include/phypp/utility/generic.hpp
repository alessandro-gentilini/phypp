#ifndef PHYPP_UTILITY_GENERIC_HPP
#define PHYPP_UTILITY_GENERIC_HPP

#include "phypp/core/vec.hpp"
#include "phypp/core/meta.hpp"
#include "phypp/core/range.hpp"
#include "phypp/core/error.hpp"
#include "phypp/core/string_conversion.hpp"

namespace phypp {
    // Create vectors a la IDL.
    template<typename T, typename ... Dims>
    vec<meta::dim_total<Dims...>::value, T> arr(Dims&& ... ds) {
        return vec<meta::dim_total<Dims...>::value, T>(std::forward<Dims>(ds)...);
    }

    template<typename ... Dims>
    auto fltarr(Dims ... ds) -> decltype(arr<float>(ds...)) {
        return arr<float>(ds...);
    }

    template<typename ... Dims>
    auto dblarr(Dims ... ds) -> decltype(arr<double>(ds...)) {
        return arr<double>(ds...);
    }

    template<typename ... Dims>
    auto intarr(Dims ... ds) -> decltype(arr<int_t>(ds...)) {
        return arr<int_t>(ds...);
    }

    template<typename ... Dims>
    auto uintarr(Dims ... ds) -> decltype(arr<uint_t>(ds...)) {
        return arr<uint_t>(ds...);
    }

    template<typename ... Dims>
    auto strarr(Dims ... ds) -> decltype(arr<std::string>(ds...)) {
        return arr<std::string>(ds...);
    }

    template<typename ... Dims>
    auto bytarr(Dims ... ds) -> decltype(arr<char>(ds...)) {
        return arr<char>(ds...);
    }

    template<typename ... Dims>
    auto boolarr(Dims ... ds) -> decltype(arr<bool>(ds...)) {
        return arr<bool>(ds...);
    }

    // Generate linearly increasing values.
    namespace impl {
        template<typename T, typename ... Dims>
        auto indgen_(Dims&& ... ds) -> decltype(arr<T>(std::forward<Dims>(ds)...)) {
            auto v = arr<T>(std::forward<Dims>(ds)...);
            for (uint_t i = 0; i < v.size(); ++i) {
                v.safe[i] = i;
            }
            return v;
        }
    }

    template<typename ... Dims>
    auto findgen(Dims&& ... ds) -> decltype(impl::indgen_<float>(std::forward<Dims>(ds)...)) {
        return impl::indgen_<float>(std::forward<Dims>(ds)...);
    }

    template<typename ... Dims>
    auto dindgen(Dims&& ... ds) -> decltype(impl::indgen_<double>(std::forward<Dims>(ds)...)) {
        return impl::indgen_<double>(std::forward<Dims>(ds)...);
    }

    template<typename ... Dims>
    auto indgen(Dims&& ... ds) -> decltype(impl::indgen_<int_t>(std::forward<Dims>(ds)...)) {
        return impl::indgen_<int_t>(std::forward<Dims>(ds)...);
    }

    template<typename ... Dims>
    auto uindgen(Dims&& ... ds) -> decltype(impl::indgen_<uint_t>(std::forward<Dims>(ds)...)) {
        return impl::indgen_<uint_t>(std::forward<Dims>(ds)...);
    }

    // Count the total number of elements in a vector.
    template<std::size_t Dim, typename T>
    uint_t n_elements(const vec<Dim,T>& v) {
        return v.data.size();
    }

    template<typename T, typename enable = typename std::enable_if<!meta::is_vec<T>::value>::type>
    uint_t n_elements(const T& v) {
        return 1;
    }

    // Get the dimensions of a vector
    template<std::size_t Dim, typename T>
    vec1u dim(const vec<Dim,T>& v) {
        vec1u d = uintarr(Dim);
        for (uint_t i = 0; i < Dim; ++i) {
            d.safe[i] = v.dims[i];
        }
        return d;
    }

    template<typename T, typename enable = typename std::enable_if<!meta::is_vec<T>::value>::type>
    vec1u dim(const T& t) {
        return {1u};
    }

    // Get multi-dim IDs from a flat ID
    template<std::size_t D, typename T>
    vec1u mult_ids(const vec<D,T>& v, uint_t i) {
        vec1u r(D);

        for (uint_t j : range(D)) {
            r.safe[D-1-j] = i % v.dims[D-1-j];
            i /= v.dims[D-1-j];
        }

        return r;
    }

    template<std::size_t D>
    vec1u mult_ids(const std::array<uint_t,D>& dims, uint_t i) {
        vec1u r(D);

        for (uint_t j : range(D)) {
            r.safe[D-1-j] = i % dims[D-1-j];
            i /= dims[D-1-j];
        }

        return r;
    }

    // Get flat ID from multi-dim IDs
    namespace impl {
        template<std::size_t D, typename T>
        uint_t flat_id_(const vec<D,T>& v, uint_t ret, meta::cte_t<D>) {
            return ret;
        }

        template<std::size_t D, typename T, std::size_t I, typename U, typename ... Args>
        uint_t flat_id_(const vec<D,T>& v, uint_t ret, meta::cte_t<I>, U i, Args&& ... args) {
            return flat_id_(v, ret + v.pitch(I)*v.template to_idx<I>(i), meta::cte_t<I+1>{},
                std::forward<Args>(args)...);
        }
    }

    template<std::size_t D, typename T, typename ... Args>
    uint_t flat_id(const vec<D,T>& v, Args&& ... args) {
        static_assert(sizeof...(Args) == D, "wrong number of IDs provided");
        return impl::flat_id_(v, 0, meta::cte_t<0>{}, std::forward<Args>(args)...);
    }


    template<typename T, typename U, typename ... Args>
    bool same_size(const T& v1, const U& v2) {
        return n_elements(v1) && n_elements(v2);
    }

    template<typename T, typename U, typename ... Args>
    bool same_size(const T& v1, const U& v2, const Args& ... args) {
        return n_elements(v1) && n_elements(v2) && same_size(v1, args...);
    }

    namespace impl {
        inline bool same_dims_or_scalar_(uint_t size) {
            return true;
        }

        template<std::size_t Dim, typename T, typename ... Args>
        bool same_dims_or_scalar_(uint_t size, const vec<Dim,T>& v1, const Args& ... args);

        template<typename T, typename ... Args,
            typename enable = typename std::enable_if<!meta::is_vec<T>::value>::type>
        bool same_dims_or_scalar_(uint_t size, const T& v1, const Args& ... args) {
            return same_dims_or_scalar_(size, args...);
        }

        template<std::size_t Dim, typename T, typename ... Args>
        bool same_dims_or_scalar_(uint_t size, const vec<Dim,T>& v1, const Args& ... args) {
            return v1.size() == size && same_dims_or_scalar_(size, args...);
        }

        inline uint_t same_dims_or_scalar_get_size_() {
            return 0u;
        }

        template<std::size_t Dim, typename T, typename ... Args>
        uint_t same_dims_or_scalar_get_size_(const vec<Dim,T>& v1, const Args& ... args) {
            return v1.size();
        }

        template<typename T, typename ... Args,
            typename enable = typename std::enable_if<!meta::is_vec<T>::value>::type>
        uint_t same_dims_or_scalar_get_size_(const T& v1, const Args& ... args) {
            return same_dims_or_scalar_get_size_(args...);
        }
    }

    template<typename ... Args>
    bool same_dims_or_scalar(const Args& ... args) {
        uint_t size = impl::same_dims_or_scalar_get_size_(args...);
        return size == 0 || impl::same_dims_or_scalar_(size, args...);
    }

    template<std::size_t Dim, typename T>
    auto element(const vec<Dim,T>& v) -> decltype(v.safe[0]) {
        phypp_check(!v.empty(), "cannot get element of empty array");
        return v.safe[0];
    }

    template<typename T, typename enable = typename std::enable_if<!meta::is_vec<T>::value>::type>
    T& element(T& v) {
        return v;
    }

    template<typename T>
    auto first(const vec<1,T>& v) -> decltype(v.safe[0]) {
        phypp_check(!v.empty(), "cannot get first element of empty array");
        return v.safe[0];
    }

    template<typename T>
    auto last(const vec<1,T>& v) -> decltype(v.safe[0]) {
        phypp_check(!v.empty(), "cannot get last element of empty array");
        return v.safe[v.data.size()-1];
    }

    // Return the indices of the vector where the value is 'true'.
    template<std::size_t Dim, typename Type, typename enable =
        typename std::enable_if<std::is_same<meta::rtype_t<Type>,bool>::value>::type>
    vec1u where(const vec<Dim,Type>& v) {
        vec1u ids;
        ids.data.reserve(n_elements(v));
        for (uint_t i : range(v)) {
            if (v.safe[i]) {
                ids.data.push_back(i);
            }
        }

        ids.dims[0] = ids.data.size();
        return ids;
    }

    // Returns the position of the first value in the vector that is 'true'
    // Returns 'npos' if no value is found
    template<std::size_t Dim, typename Type, typename enable =
        typename std::enable_if<std::is_same<meta::rtype_t<Type>,bool>::value>::type>
    uint_t where_first(const vec<Dim,Type>& v) {
        if (v.empty()) return npos;

        for (uint_t i : range(v)) {
            if (v.safe[i]) {
                return i;
            }
        }

        return npos;
    }

    // Returns the position of the last value in the vector that is 'true'
    // Returns 'npos' if no value is found
    template<std::size_t Dim, typename Type, typename enable =
        typename std::enable_if<std::is_same<meta::rtype_t<Type>,bool>::value>::type>
    uint_t where_last(const vec<Dim,Type>& v) {
        if (v.empty()) return npos;

        for (uint_t i : range(v)) {
            if (v.safe[v.size()-1-i]) {
                return v.size()-1-i;
            }
        }

        return npos;
    }

    // Build the complement of a set of indices, i.e. return the indices that are not part of the
    // provided set.
    template<std::size_t Dim, typename Type>
    vec1u complement(const vec<Dim,Type>& v, const vec1u& ids) {
        if (ids.size() == v.size()) return vec1u();
        phypp_check(ids.size() < v.size(), "incompatible size of ids (", ids.size(),
            " vs. ", v.size(), ")");

        vec1b sel(v.size());
        for (uint_t i : ids) {
            sel[i] = true;
        }

        vec1u res; res.reserve(v.size() - ids.size());
        for (uint_t i : range(v)) {
            if (!sel.safe[i]) {
                res.push_back(i);
            }
        }

        return res;
    }

    // In a sorted vector, return the first indices of each non unique sequence, effectively returning
    // indices to all values that are different in the vector.
    // By construction, the returned indices point to sorted values in the original vector.
    template<std::size_t Dim, typename Type>
    vec1u uniq(const vec<Dim,Type>& v) {
        vec1u r;
        if (v.empty()) return r;
        r.reserve(v.size()/4);

        meta::rtype_t<Type> last = v.safe[0];
        r.push_back(0);
        for (uint_t i : range(1, v.size())) {
            if (v.safe[i] != last) {
                r.push_back(i);
                last = v.safe[i];
            }
        }

        r.data.shrink_to_fit();
        return r;
    }

    // In a vector, return indices to all values that are different. This version takes a second
    // argument with indices that sort the input vector.
    // The returned indices point to sorted values in the original vector.
    template<std::size_t Dim, typename Type>
    vec1u uniq(const vec<Dim,Type>& v, const vec1u& sid) {
        vec1u r;
        if (sid.empty()) return r;
        r.reserve(v.size()/4);

        meta::rtype_t<Type> last = v[sid[0]];
        r.push_back(sid[0]);
        for (uint_t ti : range(1, sid.size())) {
            uint_t i = sid.safe[ti];
            if (v[i] != last) {
                r.push_back(i);
                last = v.safe[i];
            }
        }

        r.data.shrink_to_fit();
        return r;
    }

    // For each value of the first vector, return 'true' if it is equal to any of the values of the
    // second vector, and 'false' otherwise.
    template<typename Type1, std::size_t Dim2, typename Type2 = Type1>
    bool is_any_of(const Type1& v1, const vec<Dim2,Type2>& v2) {
        for (uint_t j : range(v2)) {
            if (v1 == v2.safe[j]) {
                return true;
            }
        }

        return false;
    }

    // For each value of the first vector, return 'true' if it is equal to any of the values of the
    // second vector, and 'false' otherwise.
    template<std::size_t Dim1, typename Type1, std::size_t Dim2 = Dim1, typename Type2 = Type1>
    vec<Dim1,bool> is_any_of(const vec<Dim1,Type1>& v1, const vec<Dim2,Type2>& v2) {
        vec<Dim1,bool> r(v1.dims);
        for (uint_t i : range(v1))
        for (uint_t j : range(v2)) {
            if (v1.safe[i] == v2.safe[j]) {
                r.safe[i] = true;
                break;
            }
        }

        return r;
    }

    // Compare the two provided vectors and push indices where the two match into 'id1' and 'id2'.
    // If both 'v1' and 'v2' are only composed of unique values, this function is symmetric. Else,
    // only the index of the first value of 'v2' that matches that of 'v1' is stored.
    template<std::size_t D1, std::size_t D2, typename Type1, typename Type2>
    void match(const vec<D1,Type1>& v1, const vec<D2,Type2>& v2, vec1u& id1, vec1u& id2) {
        // TODO: (optimization) sort v2 and use a binary search
        // must find the right criterion on each vector's size to see
        // when this is profitable

        uint_t n1 = v1.size();
        uint_t n2 = v2.size();
        uint_t n = std::min(n1, n2);

        id1.data.reserve(n/2 + id1.size());
        id2.data.reserve(n/2 + id2.size());

        for (uint_t i : range(n1))
        for (uint_t j : range(n2)) {
            if (v2.safe[j] == v1.safe[i]) {
                id1.data.push_back(i);
                id2.data.push_back(j);
                break;
            }
        }

        id1.dims[0] = id1.data.size();
        id1.data.shrink_to_fit();
        id2.dims[0] = id2.data.size();
        id2.data.shrink_to_fit();
    }

    template<typename T, typename enable = typename std::enable_if<!meta::is_vec<T>::value>::type>
    T flatten(T&& t) {
        return t;
    }

    template<std::size_t Dim, typename Type>
    vec<1,Type> flatten(const vec<Dim,Type>& v) {
        vec<1,Type> r;
        r.dims[0] = v.data.size();
        r.data = v.data;
        return r;
    }

    template<std::size_t Dim, typename Type>
    vec<1,Type> flatten(vec<Dim,Type>&& v) {
        vec<1,Type> r;
        r.dims[0] = v.data.size();
        r.data = std::move(v.data);
        return r;
    }

    template<std::size_t Dim, typename Type>
    vec<1,Type*> flatten(const vec<Dim,Type*>& v) {
        vec<1,Type*> r(impl::vec_ref_tag, v.parent);
        r.dims[0] = v.data.size();
        r.data = v.data;
        return r;
    }

    template<std::size_t Dim, typename Type>
    vec<1,Type*> flatten(vec<Dim,Type*>&& v) {
        vec<1,Type*> r(impl::vec_ref_tag, v.parent);
        r.dims[0] = v.data.size();
        r.data = std::move(v.data);
        return r;
    }

    template<std::size_t Dim, typename Type, typename ... Args>
    vec<meta::dim_total<Args...>::value, Type> reform(const vec<Dim,Type>& v, Args&& ... args) {
        vec<meta::dim_total<Args...>::value, Type> r;
        impl::set_array(r.dims, std::forward<Args>(args)...);
        uint_t nsize = 1;
        for (uint_t i : range(meta::dim_total<Args...>::value)) {
            nsize *= r.dims[i];
        }

        phypp_check(v.size() == nsize,
            "incompatible dimensions (", v.dims, " vs ", r.dims, ")");

        r.data = v.data;

        return r;
    }

    template<std::size_t Dim, typename Type, typename ... Args>
    vec<meta::dim_total<Args...>::value, Type> reform(vec<Dim,Type>&& v, Args&& ... args) {
        vec<meta::dim_total<Args...>::value, Type> r;
        impl::set_array(r.dims, std::forward<Args>(args)...);
        uint_t nsize = 1;
        for (uint_t i : range(meta::dim_total<Args...>::value)) {
            nsize *= r.dims[i];
        }

        phypp_check(v.size() == nsize,
            "incompatible dimensions ("+strn(v.dims)+" vs "+strn(r.dims)+")");

        r.data = std::move(v.data);

        return r;
    }

    template<std::size_t Dim, typename Type, typename ... Args>
    vec<meta::dim_total<Args...>::value, Type*> reform(const vec<Dim,Type*>& v, Args&& ... args) {
        vec<meta::dim_total<Args...>::value, Type*> r(impl::vec_ref_tag, v.parent);
        impl::set_array(r.dims, std::forward<Args>(args)...);
        uint_t nsize = 1;
        for (uint_t i : range(meta::dim_total<Args...>::value)) {
            nsize *= r.dims[i];
        }

        phypp_check(v.size() == nsize,
            "incompatible dimensions ("+strn(v.dims)+" vs "+strn(r.dims)+")");

        r.data = v.data;

        return r;
    }

    template<std::size_t Dim, typename Type, typename ... Args>
    vec<meta::dim_total<Args...>::value, Type*> reform(vec<Dim,Type*>&& v, Args&& ... args) {
        vec<meta::dim_total<Args...>::value, Type*> r(impl::vec_ref_tag, v.parent);
        impl::set_array(r.dims, std::forward<Args>(args)...);
        uint_t nsize = 1;
        for (uint_t i : range(meta::dim_total<Args...>::value)) {
            nsize *= r.dims[i];
        }

        phypp_check(v.size() == nsize,
            "incompatible dimensions ("+strn(v.dims)+" vs "+strn(r.dims)+")");

        r.data = std::move(v.data);

        return r;
    }

    template<typename Type>
    vec<1,Type> reverse(vec<1,Type> v) {
        std::reverse(v.data.begin(), v.data.end());
        return v;
    }

    template<typename Type>
    vec<2,Type> transpose(const vec<2,Type>& v) {
        vec<2,Type> r(impl::vec_nocopy_tag, v);
        std::swap(r.dims[0], r.dims[1]);

        for (uint_t i : range(v)) {
            r.data.push_back(v.data[(i%v.dims[0])*v.dims[1] + i/v.dims[0]]);
        }

        // TODO: (optimization) see who's faster
        // r.resize();
        // for (uint_t i : range(r.dims[0]))
        // for (uint_t j : range(r.dims[1])) {
        //     r.data[j+i*r.dims[1]] = v.data[i+j*v.dims[1]];
        // }

        return r;
    }

    template<std::size_t Dim, typename Type = double, typename ... Args>
    vec<Dim+meta::dim_total<Args...>::value, meta::rtype_t<Type>>
        replicate(const vec<Dim,Type>& t, Args&& ... args) {
        static const std::size_t FDim = Dim+meta::dim_total<Args...>::value;
        vec<FDim, meta::rtype_t<Type>> v(std::forward<Args>(args)..., t.dims);

        std::size_t pitch = t.size();
        std::size_t n = v.size()/pitch;
        for (uint_t i : range(n))
        for (uint_t j : range(pitch)) {
            v.safe[i*pitch + j] = t.safe[j];
        }

        return v;
    }

    template<typename Type, typename ... Args>
    vec<meta::dim_total<Args...>::value, meta::vtype_t<Type>> replicate(const Type& t, Args&& ... args) {
        static const std::size_t FDim = meta::dim_total<Args...>::value;
        vec<FDim, meta::vtype_t<Type>> v(std::forward<Args>(args)...);

        for (auto& e : v) {
            e = t;
        }

        return v;
    }

    template<std::size_t Dim, typename Type>
    vec1u sort(const vec<Dim,Type>& v) {
        vec1u r = uindgen(v.size());
        std::stable_sort(r.data.begin(), r.data.end(), [&v](uint_t i, uint_t j) {
            return typename vec<Dim,Type>::comparator()(v.data[i], v.data[j]);
        });

        return r;
    }

    template<std::size_t Dim, typename Type, typename F>
    vec1u sort(const vec<Dim,Type>& v, F&& comp) {
        vec1u r = uindgen(v.size());
        std::stable_sort(r.data.begin(), r.data.end(), [&v,&comp](uint_t i, uint_t j) {
            return comp(v.safe[i], v.safe[j]);
        });

        return r;
    }

    template<std::size_t Dim, typename Type>
    void inplace_sort(vec<Dim,Type>& v) {
        std::stable_sort(v.data.begin(), v.data.end(), typename vec<Dim,Type>::comparator());
    }

    template<std::size_t Dim, typename Type, typename F>
    void inplace_sort(vec<Dim,Type>& v, F&& comp) {
        std::stable_sort(v.begin(), v.end(), std::forward<F>(comp));
    }

    // Check if a given array is sorted or not
    template<std::size_t Dim, typename Type>
    bool is_sorted(const vec<Dim,Type>& v) {
        for (uint_t i : range(1, v.size())) {
            // Note: watch out for NaN values!
            // Make sure to use a test that returns 'false' if NaN is involved
            if (v.safe[i-1] >= v.safe[i]) return false;
        }

        return true;
    }

    // Returns the position of the last value in the array that is less than or equal to 'x'.
    // Returns 'npos' if no value satisfy this criterium.
    // Note: assumes that 'v' is sorted and does not contain NaN values.
    template<typename T, std::size_t Dim, typename Type>
    uint_t lower_bound(T x, const vec<Dim,Type>& v) {
        if (v.empty()) return npos;

        auto iter = std::upper_bound(v.data.begin(), v.data.end(), x,
            typename vec<Dim,Type>::comparator());

        if (iter == v.data.begin()) {
            return npos;
        } else {
            return iter - v.data.begin() - 1;
        }
    }

    // Returns the position of the first value in the array that is greater than 'x'.
    // Returns 'npos' if no value satisfy this criterium.
    // Note: assumes that 'v' is sorted and does not contain NaN values.
    template<typename T, std::size_t Dim, typename Type>
    uint_t upper_bound(T x, const vec<Dim,Type>& v) {
        if (v.empty()) return npos;

        auto iter = std::upper_bound(v.data.begin(), v.data.end(), x,
            typename vec<Dim,Type>::comparator());

        if (iter == v.data.end()) {
            return npos;
        } else {
            return iter - v.data.begin();
        }
    }

    // Return the position of the last value in 'v' that is less than or equal to 'x' and
    // the position of the first value in 'v' that is greater than 'x'.
    // Note: assumes that 'v' is sorted and does not contain NaN values.
    template<typename T, std::size_t Dim, typename Type>
    std::array<uint_t,2> bounds(T x, const vec<Dim,Type>& v) {
        std::array<uint_t,2> res;

        if (v.empty()) {
            res[0] = npos;
            res[1] = npos;
        } else {
            auto iter = std::upper_bound(v.data.begin(), v.data.end(), x,
                typename vec<Dim,Type>::comparator());

            if (iter == v.data.end()) {
                res[0] = v.size() - 1;
                res[1] = npos;
            } else if (iter == v.data.begin()) {
                res[0] = npos;
                res[1] = 0;
            } else {
                res[1] = iter - v.data.begin();
                res[0] = res[1] - 1;
            }
        }

        return res;
    }

    // Return the position of the last value in 'v' that is less than or equal to 'x1' and
    // the position of the first value in 'v' that is greater than 'x2'.
    // Note: assumes that:
    //  1) 'v' is sorted and does not contain NaN values,
    //  2) 'x2' is greater than or equal to 'x1'.
    template<typename T, typename U, std::size_t Dim, typename Type>
    std::array<uint_t,2> bounds(T x1, U x2, const vec<Dim,Type>& v) {
        std::array<uint_t,2> res;

        if (v.empty()) {
            res[0] = npos;
            res[1] = npos;
        } else {
            auto iter = std::upper_bound(v.data.begin(), v.data.end(), x1,
                typename vec<Dim,Type>::comparator());

            if (iter == v.data.begin()) {
                res[0] = npos;
            } else {
                res[0] = iter - v.data.begin() - 1;
            }

            iter = std::upper_bound(iter, v.data.end(), x2,
                typename vec<1,Type>::comparator());

            if (iter == v.data.end()) {
                res[1] = npos;
            } else {
                res[1] = iter - v.data.begin();
            }
        }

        return res;
    }

    // Return the indices of all the values in the array that are equal to 'x'.
    // Note: assumes that 'v' is sorted and does not contain NaN values.
    template<typename T, std::size_t Dim, typename Type>
    vec1u equal_range(T x, const vec<Dim,Type>& v) {
        auto res = std::equal_range(v.data.begin(), v.data.end(), x,
            typename vec<Dim,Type>::comparator());

        return uindgen(1 + (res.second - res.first)) + (res.first - v.data.begin());
    }

    template<std::size_t Dim, typename Type>
    void inplace_remove(vec<Dim,Type>& v, vec1u ids) {
        inplace_sort(ids);
        uint_t i = 0;
        uint_t pitch = v.pitch(0);
        uint_t osize = v.size();
        while (i < ids.size()) {
            uint_t i1 = ids.safe[ids.size()-1-i];
            uint_t i0 = i1;

            phypp_check(i1 < osize, "trying to erase value ", i1, " in vector of "
                "dimensions ", v.dims);

            ++i;
            while (i < ids.size() && i0 - ids.safe[ids.size()-1-i] <= 1) {
                i0 = ids.safe[ids.size()-1-i];

                phypp_check(i0 != i1, "remove indices contain duplicates");
                phypp_check(i0 < osize, "trying to erase value ", i0, " in vector of "
                    "dimensions ", v.dims);

                ++i;
            }

            v.data.erase(v.data.begin()+i0*pitch, v.data.begin()+(i1+1)*pitch);
        }

        v.dims[0] -= ids.size();
    }

    template<std::size_t Dim, typename Type>
    vec<Dim,Type> remove(vec<Dim,Type> v, const vec1u& ids) {
        inplace_remove(v, ids);
        return v;
    }

    template<std::size_t N, std::size_t Dim, typename Type1, typename Type2 = Type1,
        typename enable = typename std::enable_if<(N < Dim)>::type>
    void append(vec<Dim,Type1>& t1, const vec<Dim,Type2>& t2) {
        if (t1.empty()) {
            t1 = t2;
            return;
        }

        if (t2.empty()) return;

        std::size_t n1 = t1.dims[N], n2 = t2.dims[N];
        phypp_check(t1.size()/n1 == t2.size()/n2, "cannot append dimension ", N, " in (", t1.dims,
            ") and (", t2.dims, ")");

        // TODO: (optimization) no need for this copy?
        auto tmp = t1;
        t1.dims[N] += n2;
        t1.resize();

        t1(repeat<N>(_), uindgen(n1), repeat<Dim-N-1>(_)) = tmp;
        t1(repeat<N>(_), n1+uindgen(n2), repeat<Dim-N-1>(_)) = t2;
    }

    template<std::size_t N, std::size_t Dim, typename Type1, typename Type2 = Type1,
        typename enable = typename std::enable_if<(N < Dim)>::type>
    void prepend(vec<Dim,Type1>& t1, const vec<Dim,Type2>& t2) {
        if (t1.empty()) {
            t1 = t2;
            return;
        }

        if (t2.empty()) return;

        std::size_t n1 = t1.dims[N], n2 = t2.dims[N];
        phypp_check(t1.size()/n1 == t2.size()/n2, "cannot prepend dimension ", N, " in (", t1.dims,
            ") and (", t2.dims, ")");

        // TODO: (optimization) no need for this copy?
        auto tmp = t1;
        t1.dims[N] += n2;
        t1.resize();

        t1(repeat<N>(_), uindgen(n2), repeat<Dim-N-1>(_)) = t2;
        t1(repeat<N>(_), n2+uindgen(n1), repeat<Dim-N-1>(_)) = tmp;
    }

    template<typename Type1, typename Type2 = Type1>
    void append(vec<1,Type1>& t1, const vec<1,Type2>& t2) {
        t1.data.insert(t1.data.end(), t2.begin(), t2.end());
        t1.dims[0] += t2.dims[0];
    }

    template<typename Type1, typename Type2 = Type1>
    void prepend(vec<1,Type1>& t1, const vec<1,Type2>& t2) {
        t1.data.insert(t1.data.begin(), t2.begin(), t2.end());
        t1.dims[0] += t2.dims[0];
    }

    template<typename Type>
    vec<1,meta::rtype_t<Type>> shift(const vec<1,Type>& v, int_t n, meta::rtype_t<Type> def = 0) {
        vec<1,meta::rtype_t<Type>> tmp;

        if (uint_t(std::abs(n)) > v.dims[0]) {
            tmp = replicate(def, v.dims[0]);
            return tmp;
        }

        tmp = v.concretise();

        if (n < 0) {
            tmp.data.erase(tmp.data.begin(), tmp.data.begin()-n);
            tmp.data.insert(tmp.data.end(), -n, def);
        } else if (n > 0) {
            tmp.data.erase(tmp.data.end()-n, tmp.data.end());
            tmp.data.insert(tmp.data.begin(), n, def);
        }

        return tmp;
    }

    // Find the closest point in a 2D array that satisfies a given criterium
    inline bool astar_find(const vec2b& map, uint_t& x, uint_t& y) {
        phypp_check(!map.empty(), "this algorithm requires a non empty 2D vector");

        if (x >= map.dims[0]) x = map.dims[0]-1;
        if (y >= map.dims[1]) y = map.dims[1]-1;

        if (map.safe(x,y)) return true;

        using vec_pair = vec<1,std::pair<uint_t,uint_t>>;
        vec_pair open;
        open.push_back(std::make_pair(x,y));

        vec2b visit(map.dims);
        visit.safe(x,y) = true;

        while (!open.empty()) {
            vec_pair old_open = std::move(open);

            for (auto p : old_open) {
                int_t ox = p.first, oy = p.second;

                for (uint_t d : range(4)) {
                    int_t tnx, tny;
                    if (d == 0) {
                        tnx = ox;   tny = oy+1;
                    } else if (d == 1) {
                        tnx = ox+1; tny = oy;
                    } else if (d == 2) {
                        tnx = ox;   tny = oy-1;
                    } else {
                        tnx = ox-1; tny = oy;
                    }

                    if (tnx < 0 || tny < 0) continue;

                    x = tnx, y = tny;
                    if (x >= map.dims[0] || y >= map.dims[1] || visit.safe(x,y)) continue;

                    if (!map.safe(x,y)) {
                        open.push_back(std::make_pair(x,y));
                        visit.safe(x,y) = true;
                    } else {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // Create an overloaded lambda that supports both scalars and vectors for the first argument.
    namespace impl {
        template<typename L>
        struct vectorized_lambda_first_t {
            L lambda;

            vectorized_lambda_first_t(L tlam) : lambda(tlam) {}

            template<typename T, typename ... Args, typename enable =
                typename std::enable_if<!meta::is_vec<typename std::decay<T>::type>::value>::type>
            auto operator()(T&& t, Args&& ... args) ->
                decltype(lambda(std::forward<T>(t), std::forward<Args>(args)...)) {
                return lambda(std::forward<T>(t), std::forward<Args>(args)...);
            }

            template<typename T, std::size_t D, typename ... Args>
            auto operator()(const vec<D,T>& t, Args&& ... args) ->
                vec<D,typename std::decay<decltype(lambda(std::declval<const meta::rtype_t<T>&>(),
                    std::forward<Args>(args)...))>::type> {
                vec<D,typename std::decay<decltype(lambda(std::declval<const meta::rtype_t<T>&>(),
                    std::forward<Args>(args)...))>::type> ret(t.dims);
                for (uint_t i : range(t)) {
                    ret.safe[i] = lambda(t.safe[i], std::forward<Args>(args)...);
                }
                return ret;
            }

            template<typename T, std::size_t D, typename ... Args>
            auto operator()(vec<D,T>&& t, Args&& ... args) ->
                vec<D,typename std::decay<decltype(lambda(std::declval<meta::rtype_t<T>>(),
                    std::forward<Args>(args)...))>::type> {
                vec<D,typename std::decay<decltype(lambda(std::declval<meta::rtype_t<T>>(),
                    std::forward<Args>(args)...))>::type> ret(t.dims);
                for (uint_t i : range(t)) {
                    ret.safe[i] = lambda(std::move(t.safe[i]), std::forward<Args>(args)...);
                }
                return ret;
            }
        };
    }

    template<typename T>
    impl::vectorized_lambda_first_t<typename std::decay<T>::type> vectorize_lambda_first(T&& t) {
        return impl::vectorized_lambda_first_t<typename std::decay<T>::type>(std::move(t));
    }

    // Create an overloaded lambda that supports either scalars or vectors for all arguments.
    // If multiple vectors are found in the argument list, they are iterated jointly and
    // therefore must have the same dimensions.
    namespace impl {
        template<typename T>
        struct elem_dim : std::integral_constant<std::size_t, 0> {};

        template<std::size_t D, typename T>
        struct elem_dim<vec<D,T>> : std::integral_constant<std::size_t, D> {};

        template<typename ... Args>
        using common_dim = meta::max<std::size_t, elem_dim<typename std::decay<Args>::type>::value...>;

        template <std::size_t D>
        struct has_right_dim {
            template <typename T>
            struct type : meta::bool_constant<elem_dim<T>::value == D || elem_dim<T>::value == 0> {};
        };

        template <typename T, typename U>
        void lambda_check_dims(T& dims, bool& set, const U& u) {}

        template <typename T, std::size_t D, typename U>
        void lambda_check_dims(T& dims, bool& set, const vec<D,U>& u) {
            if (!set) {
                dims = u.dims;
                set = true;
            } else {
                phypp_check(dims == u.dims, "incompatible dimensions in lambda call (",
                    dims, " vs ", u.dims, ")");
            }
        }

        template<typename L>
        struct vectorize_lambda_t {
            L lambda;

            vectorize_lambda_t(L tlam) : lambda(tlam) {}

            // Get 'i'th element from argument
            template <typename T, typename ... Args>
            static T& get(uint_t i, T& t) {
                return t;
            }

            template <typename T, typename ... Args>
            static const T& get(uint_t i, const T& t) {
                return t;
            }

            template <std::size_t D, typename T, typename ... Args>
            static auto get(uint_t i, vec<D,T>& t) -> decltype(t.safe[i]) {
                return t.safe[i];
            }

            template <std::size_t D, typename T, typename ... Args>
            static auto get(uint_t i, const vec<D,T>& t) -> decltype(t.safe[i]) {
                return t.safe[i];
            }

            template <typename ... Args>
            static void swallow(Args&&...) {}

            // Bake return type
            template <typename ... Args>
            using scalar_return_type = decltype(std::declval<L>()(get(0, std::declval<Args>())...));

            template <typename ... Args>
            using vector_return_type = vec<common_dim<Args...>::value, scalar_return_type<Args...>>;

            // Full scalar call
            // ----------------
            template <typename ... Args>
            void run(std::true_type, std::true_type, Args&& ... args) {
                lambda(std::forward<Args>(args)...);
            }

            template <typename ... Args>
            scalar_return_type<Args...> run(std::true_type, std::false_type, Args&& ... args) {
                return lambda(std::forward<Args>(args)...);
            }

            // Vectorized call
            // ----------------

            template <typename ... Args>
            void run(std::false_type, std::true_type, Args&& ... args) {
                constexpr const std::size_t D = common_dim<Args...>::value;
                static_assert(meta::are_all_true<meta::bool_list<has_right_dim<D>::template type<Args>::value...>>::value,
                    "incompatible number of dimensions in lambda call");

                bool set = false;
                std::array<std::size_t,D> dims; // only used to get the dimensions
                swallow((lambda_check_dims(dims, set, args), 0)...);

                std::size_t size = 1;
                for (uint_t i : range(D)) {
                    size *= dims[i];
                }

                for (uint_t i : range(size)) {
                    lambda(get(i, args)...);
                }
            }

            template <typename ... Args>
            vector_return_type<Args...> run(std::false_type, std::false_type, Args&& ... args) {
                vector_return_type<Args...> ret;
                constexpr const std::size_t D = common_dim<Args...>::value;
                static_assert(meta::are_all_true<meta::bool_list<has_right_dim<D>::template type<Args>::value...>>::value,
                    "incompatible number of dimensions in lambda call");

                bool set = false;
                swallow((lambda_check_dims(ret.dims, set, args), 0)...);

                ret.resize();

                for (uint_t i : range(ret)) {
                    ret.safe[i] = lambda(get(i, args)...);
                }

                return ret;
            }

            // Generic call dispatcher
            // -----------------------

            // Needs to check if:
            // 1) all the parameters are scalars, to return a scalar or a vector
            // 2) the return value is void, to avoid creating a return value
            template<typename ... Args>
            auto operator()(Args&& ... args) ->
                decltype(this->run(meta::bool_constant<common_dim<Args...>::value == 0>{},
                             std::is_same<scalar_return_type<Args...>, void>{},
                             std::forward<Args>(args)...)) {
                return this->run(meta::bool_constant<common_dim<Args...>::value == 0>{},
                           std::is_same<scalar_return_type<Args...>, void>{},
                           std::forward<Args>(args)...);
            }
        };
    }

    template<typename T>
    impl::vectorize_lambda_t<typename std::decay<T>::type> vectorize_lambda(T&& t) {
        return impl::vectorize_lambda_t<typename std::decay<T>::type>(std::move(t));
    }

    // Increment a list of indices, step by step
    // This is an alternative to a recursive loop.
    inline void increment_index_list(vec1u& ids, const vec1u& n) {
        uint_t i = ids.size();
        do {
            --i;
            ++(ids.safe[i]);
            if (ids.safe[i] == n.safe[i]) {
                ids.safe[i] = 0;
            } else {
                break;
            }
        } while (i != 0);
    }

    inline void increment_index_list(vec1u& ids, const uint_t& n) {
        uint_t i = ids.size();
        do {
            --i;
            ++(ids.safe[i]);
            if (ids.safe[i] == n) {
                ids.safe[i] = 0;
            } else {
                break;
            }
        } while (i != 0);
    }
}

#endif
