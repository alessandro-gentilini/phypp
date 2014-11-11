#ifndef ASTRO_QXMATCH_HPP
#define ASTRO_QXMATCH_HPP

#include "phypp/astro.hpp"

struct qxmatch_res {
    vec2u id;
    vec2d d;
    vec1u rid;
    vec1d rd;

    // Reflection data
    MEMBERS1(id, d, rid, rd);
    MEMBERS2("qxmatch_res", MAKE_MEMBER(id), MAKE_MEMBER(d), MAKE_MEMBER(rid), MAKE_MEMBER(rd));
};

void qxmatch_save(const std::string& file, const qxmatch_res& r) {
    fits::write_table(file, ftable(r.id, r.d, r.rid, r.rd));
}

qxmatch_res qxmatch_restore(const std::string& file) {
    qxmatch_res r;
    fits::read_table(file, ftable(r.id, r.d, r.rid, r.rd));
    return r;
}

struct qxmatch_params {
    uint_t thread = 1u;
    uint_t nth = 1u;
    bool verbose = false;
    bool self = false;
    bool brute_force = false;
};

template<typename C1, typename C2,
    typename enable = typename std::enable_if<!is_vec<C1>::value>::type>
qxmatch_res qxmatch(const C1& cat1, const C2& cat2, qxmatch_params params = qxmatch_params{}) {
    return qxmatch(cat1.ra, cat1.dec, cat2.ra, cat2.dec, params);
}

template<typename C1, typename enable = typename std::enable_if<!is_vec<C1>::value>::type>
qxmatch_res qxmatch(const C1& cat1, qxmatch_params params = qxmatch_params{}) {
    return qxmatch(cat1.ra, cat1.dec, params);
}

template<typename TypeR1, typename TypeD1>
qxmatch_res qxmatch(const vec_t<1,TypeR1>& ra1, const vec_t<1,TypeD1>& dec1,
    qxmatch_params params = qxmatch_params{}) {
    params.self = true;
    return qxmatch(ra1, dec1, ra1, dec1, params);
}

namespace qxmatch_impl {
    struct depth_cache {
        struct depth_t {
            vec1i bx, by;
            double max_dist;
        };

        std::vector<depth_t> depths;
        vec2b                visited;
        double               csize;

        depth_cache(uint_t nx, uint_t ny, double cs) : csize(cs) {
            visited = vec2b(nx, ny);
            depths.reserve(20);

            // First depth is trivial
            depths.push_back(depth_t{});
            auto& depth = depths.back();
            depth.max_dist = csize/2.0;
            depth.bx = {0};
            depth.by = {0};
            visited(0,0) = true;

            // Generate a few in advance
            for (uint_t d : range(9)) {
                grow();
            }
        }

        const depth_t& operator[] (uint_t i) {
            if (i >= depths.size()) {
                for (uint_t d : range(i+1 - depths.size())) {
                    grow();
                }
            }

            return depths[i];
        }

        void grow() {
            depths.push_back(depth_t{});
            auto& depth = depths.back();
            uint_t d = depths.size();

            // Look further by one cell size
            depth.max_dist = csize*(d + 0.5);

            // See which new buckets are reached.
            // We only need to do the maths for one quadrant, the other 3
            // can be deduced by symmetry.
            for (uint_t x : range(1,d+1))
            for (uint_t y : range(d+1)) {
                double dist2 = sqr(csize)*min(vec1d{
                    sqr(x-0.5) + sqr(y-0.5), sqr(x+0.5) + sqr(y-0.5),
                    sqr(x+0.5) + sqr(y+0.5), sqr(x-0.5) + sqr(y+0.5)
                });

                if (dist2 <= sqr(depth.max_dist) && !visited(x, y)) {
                    depth.bx.push_back(x);
                    depth.by.push_back(y);
                    visited(x, y) = true;
                }
            }

            // We have (+x,+y), fill the other 3 quadrants:
            // (-x,+y), (-x,-y), (-x,+y)
            vec1u idnew = uindgen(depth.bx.size());
            append(depth.bx, -depth.by[idnew]);
            append(depth.by, depth.bx[idnew]);
            append(depth.bx, -depth.bx[idnew]);
            append(depth.by, -depth.by[idnew]);
            append(depth.bx, depth.by[idnew]);
            append(depth.by, -depth.bx[idnew]);
        }
    };
}

template<typename TypeR1, typename TypeD1, typename TypeR2, typename TypeD2>
qxmatch_res qxmatch(const vec_t<1,TypeR1>& ra1, const vec_t<1,TypeD1>& dec1,
    const vec_t<1,TypeR2>& ra2, const vec_t<1,TypeD2>& dec2,
    qxmatch_params params = qxmatch_params{}) {

    phypp_check(ra1.dims == dec1.dims, "first RA and Dec dimensions do not match (",
        ra1.dims, " vs ", dec1.dims, ")");
    phypp_check(ra2.dims == dec2.dims, "second RA and Dec dimensions do not match (",
        ra2.dims, " vs ", dec2.dims, ")");

    const double d2r = 3.14159265359/180.0;
    auto dra1  = ra1*d2r;
    auto ddec1 = dec1*d2r;
    auto dcdec1 = cos(ddec1);
    auto dra2  = ra2*d2r;
    auto ddec2 = dec2*d2r;
    auto dcdec2 = cos(ddec2);

    const uint_t n1 = n_elements(ra1);
    const uint_t n2 = n_elements(ra2);

    uint_t nth = clamp(params.nth, 1u, npos);

    qxmatch_res res;
    res.id = replicate(npos, nth, n1);
    res.d  = dblarr(nth, n1)+dinf;
    res.rid = replicate(npos, n2);
    res.rd  = dblarr(n2)+dinf;

    auto distance_proxy = [&](uint_t i, uint_t j) {
        // Note that this is not the 'true' distance in arseconds, but this is
        // sufficient to find the nearest neighbors (the true distance is obtained
        // by computing 2*asin(sqrt(sd))), but all these functions are monotonous,
        // hence not applying them does not change the relative distances).
        double sra = sin(0.5*(dra2[j] - dra1[i]));
        double sde = sin(0.5*(ddec2[j] - ddec1[i]));
        return sde*sde + sra*sra*dcdec2[j]*dcdec1[i];
    };

    if (!params.brute_force) {
        // Get bounds of the fields
        vec1d rra1  = {min(ra1),  max(ra1)};
        vec1d rra2  = {min(ra2),  max(ra2)};
        vec1d rdec1 = {min(dec1), max(dec1)};
        vec1d rdec2 = {min(dec2), max(dec2)};

        vec1d rra  = {std::min(rra1[0],  rra2[0]),  std::max(rra1[1],  rra2[1])};
        vec1d rdec = {std::min(rdec1[0], rdec2[0]), std::max(rdec1[1], rdec2[1])};

        // Choose a bucket size (arcsec)
        const double overgrowth = 10.0;
        uint_t nc2 = ceil(0.5*sqrt(dpi*ra2.size()/nth/overgrowth));
        vec1d hx = vec1d{rra2[0], rra2[0], rra2[1], rra2[1]};
        vec1d hy = vec1d{rdec2[0], rdec2[1], rdec2[0], rdec2[1]};
        double area2 = field_area_hull(hx, hy);
        double cell_size = 3600.0*sqrt(area2)/nc2;

        // Be careful that RA and Dec are spherical coordinates
        double dra = cell_size*fabs(cos(mean(rdec2)*dpi/180.0))/3600.0;
        double ddec = cell_size/3600.0;

        // Add some padding to prevent border issues
        rra[0] -= dra;
        rra[1] += dra;
        rdec[0] -= ddec;
        rdec[1] += ddec;

        // Final number of buckets
        uint_t nra = (rra[1] - rra[0])/dra;
        uint_t ndec = (rdec[1] - rdec[0])/ddec;

        // Build the buckets
        struct bucket_t {
            std::vector<uint_t> ids1;
            std::vector<uint_t> ids2;
        };

        vec_t<2,bucket_t> buckets(nra, ndec);

        // Fill the buckets
        vec1u idx1 = floor((ra1 - rra[0])/dra);
        vec1u idy1 = floor((dec1 - rdec[0])/ddec);
        for (uint_t i : range(ra1)) {
            buckets(idx1[i],idy1[i]).ids1.push_back(i);
        }

        vec1u idx2 = floor((ra2 - rra[0])/dra);
        vec1u idy2 = floor((dec2 - rdec[0])/ddec);
        for (uint_t j : range(ra2)) {
            buckets(idx2[j],idy2[j]).ids2.push_back(j);
        }

        // Precompute generic bucket geometry
        qxmatch_impl::depth_cache depths(nra, ndec, cell_size);

        auto dist_proj = [] (double dist) {
            return sqr(sin(dist/(3600.0*(180.0/3.14159265359)*2)));
        };

        res.id = replicate(npos, nth, n1);
        res.d  = dblarr(nth, n1)+dinf;
        res.rid = replicate(npos, n2);
        res.rd  = dblarr(n2)+dinf;

        auto work1 = [&] (uint_t i, qxmatch_impl::depth_cache& tdepths, qxmatch_res& tres) {
            int_t x0 = idx1[i];
            int_t y0 = idy1[i];

            // Compute the distance of this source from the bucket center
            double cell_dist = angdist(ra1[i], dec1[i],
                rra[0] + (x0+0.5)*dra, rdec[0] + (y0+0.5)*ddec);

            // Scan the buckets around this source until all neighbors are found
            uint_t d = 0;
            double reached_distance = 0.0;

            do {
                auto& depth = tdepths[d];
                for (uint_t b : range(depth.bx)) {
                    int_t x = x0+depth.bx[b];
                    int_t y = y0+depth.by[b];
                    if (x < 0 || uint_t(x) >= nra || y < 0 || uint_t(y) >= ndec) continue;

                    auto& bucket = buckets(uint_t(x),uint_t(y));
                    for (uint_t j : bucket.ids2) {
                        if (params.self && i == j) continue;

                        auto sd = distance_proxy(i, j);

                        // Compare this new distance to the largest one that is in the
                        // Nth nearest neighbor list. If it is lower than that, we
                        // insert it in the list, removing the old one, and sort the
                        // whole thing so that the largest distance goes as the end of
                        // the list.
                        if (sd < tres.d(nth-1,i)) {
                            tres.id(nth-1,i) = j;
                            tres.d(nth-1,i) = sd;
                            uint_t k = nth-2;
                            while (k != npos && tres.d(k,i) > tres.d(k+1,i)) {
                                std::swap(tres.d(k,i), tres.d(k+1,i));
                                std::swap(tres.id(k,i), tres.id(k+1,i));
                                --k;
                            }
                        }
                    }
                }

                reached_distance = dist_proj(
                    std::max(0.0, tdepths[d].max_dist - 2.0*cell_dist));

                ++d;
            } while (tres.d(nth-1,i) > reached_distance);
        };

        auto work2 = [&] (uint_t j, qxmatch_impl::depth_cache& tdepths, qxmatch_res& tres) {
            int_t x0 = idx2[j];
            int_t y0 = idy2[j];

            // Compute the distance of this source from the bucket center
            double cell_dist = angdist(ra2[j], dec2[j],
                rra[0] + (x0+0.5)*dra, rdec[0] + (y0+0.5)*ddec);

            // Scan the buckets around this source until all neighbors are found
            uint_t d = 0;
            double reached_distance = 0.0;

            do {
                auto& depth = tdepths[d];
                for (uint_t b : range(depth.bx)) {
                    int_t x = x0+depth.bx[b];
                    int_t y = y0+depth.by[b];
                    if (x < 0 || uint_t(x) >= nra || y < 0 || uint_t(y) >= ndec) continue;

                    auto& bucket = buckets(uint_t(x),uint_t(y));
                    for (uint_t i : bucket.ids1) {
                        double sd = distance_proxy(i, j);

                        // Just keep the nearest match
                        if (sd < tres.rd[j]) {
                            tres.rd[j] = sd;
                            tres.rid[j] = i;
                        }
                    }
                }

                reached_distance = dist_proj(
                    std::max(0.0, tdepths[d].max_dist - 2.0*cell_dist));

                ++d;
            } while (tres.rd[j] > reached_distance);
        };

        if (params.thread <= 1) {
            if (n2 < nth) {
                // We asked more neighbors than there are sources in the second catalog...
                // Lower 'nth' to prevent this from blocking the algorithm.
                nth = n2;
            }

            // When using a single thread, all the work is done in the main thread
            auto p = progress_start(n1+(params.self ? 0 : n2));
            for (uint_t i : range(ra1)) {
                work1(i, depths, res);
                if (params.verbose) progress(p, 31);
            }
            if (!params.self) {
                for (uint_t j : range(ra2)) {
                    work2(j, depths, res);
                    if (params.verbose) progress(p, 31);
                }
            }
        } else {
            // When using more than one thread, the work load is evenly separated between
            // all the available threads, such that they should more or less all end at
            // the same time.
            std::atomic<uint_t> iter(0);

            // Create the thread pool and launch the threads
            std::vector<qxmatch_res> vres(params.thread);
            for (auto& r : vres) {
                r.id = replicate(npos, nth, n1);
                r.d  = dblarr(nth, n1)+dinf;
                r.rid = replicate(npos, n2);
                r.rd  = dblarr(n2)+dinf;
            }

            if (n2 < nth) {
                // We asked more neighbors than there are sources in the second catalog...
                // Lower 'nth' to prevent this from blocking the algorithm.
                nth = n2;
            }

            vec1u tbeg1(params.thread);
            vec1u tend1(params.thread);
            vec1u tbeg2(params.thread);
            vec1u tend2(params.thread);
            auto pool = thread::pool(params.thread);
            uint_t total1 = 0;
            uint_t total2 = 0;
            uint_t assigned1 = floor(n1/float(params.thread));
            uint_t assigned2 = floor(n2/float(params.thread));
            for (uint_t t = 0; t < params.thread; ++t) {
                if (t == params.thread-1) {
                    assigned1 = n1 - total1;
                    assigned2 = n2 - total2;
                }

                tbeg1[t] = total1;
                tend1[t] = total1+assigned1;
                tbeg2[t] = total2;
                tend2[t] = total2+assigned2;

                pool[t].start([&iter, &work1, &work2, &vres,
                    t, tbeg1, tbeg2, tend1, tend2, depths, params]() mutable {
                    for (uint_t i = tbeg1[t]; i < tend1[t]; ++i) {
                        work1(i, depths, vres[t]);
                        ++iter;
                    }

                    if (!params.self) {
                        for (uint_t j = tbeg2[t]; j < tend2[t]; ++j) {
                            work2(j, depths, vres[t]);
                            ++iter;
                        }
                    }
                });

                total1 += assigned1;
                total2 += assigned2;
            }

            // Wait for the computation to finish.
            // Here the main thread does nothing except sleeping, occasionally waking
            // up once in a while to update the progress bar if any.
            uint_t niter = n1+(params.self ? 0 : n2);
            auto p = progress_start(niter);
            while (iter < niter) {
                thread::sleep_for(0.2);
                if (params.verbose) print_progress(p, iter);
            }

            // By now, all threads should have ended their tasks.
            // We must ask them to terminate nicely.
            for (auto& t : pool) {
                t.join();
            }

            // Merge back the results of each thread
            for (uint_t t = 0; t < params.thread; ++t) {
                auto ids = rgen(tend1[t] - tbeg1[t]) + tbeg1[t];
                res.id(_,ids) = vres[t].id(_,ids);
                res.d(_,ids) = vres[t].d(_,ids);

                ids = rgen(tend2[t] - tbeg2[t]) + tbeg2[t];
                res.rid[ids] = vres[t].rid[ids];
                res.rd[ids] = vres[t].rd[ids];
            }
        }
    } else {
        auto work = [&, nth] (uint_t i, uint_t j, qxmatch_res& tres) {
            double sd = distance_proxy(i, j);

            // We compare this new distance to the largest one that is in the Nth
            // nearest neighbor list. If it is lower than that, we insert it in the list,
            // removing the old one, and sort the whole thing so that the largest distance
            // goes as the end of the list.
            if (sd < tres.d(nth-1,i)) {
                tres.id(nth-1,i) = j;
                tres.d(nth-1,i) = sd;
                uint_t k = nth-2;
                while (k != npos && tres.d(k,i) > tres.d(k+1,i)) {
                    std::swap(tres.d(k,i), tres.d(k+1,i));
                    std::swap(tres.id(k,i), tres.id(k+1,i));
                    --k;
                }
            }

            // Take care of reverse search
            if (sd < tres.rd[j]) {
                tres.rid[j] = i;
                tres.rd[j] = sd;
            }
        };

        if (params.thread <= 1) {
            // When using a single thread, all the work is done in the main thread
            auto p = progress_start(n1);
            for (uint_t i = 0; i < n1; ++i) {
                for (uint_t j = 0; j < n2; ++j) {
                    if (params.self && i == j) continue;
                    work(i,j,res);
                }

                if (params.verbose) progress(p);
            }
        } else {
            // When using more than one thread, the work load is evenly separated between
            // all the available threads, such that they should more or less all end at
            // the same time.
            std::atomic<uint_t> iter(0);

            // Create the thread pool and launch the threads
            std::vector<qxmatch_res> vres(params.thread);
            for (auto& r : vres) {
                r.id = replicate(npos, nth, n1);
                r.d  = dblarr(nth, n1)+dinf;
                r.rid = replicate(npos, n2);
                r.rd  = dblarr(n2)+dinf;
            }
            vec1u tbeg(params.thread);
            vec1u tend(params.thread);
            auto pool = thread::pool(params.thread);
            uint_t total = 0;
            uint_t assigned = floor(n1/float(params.thread));
            for (uint_t t = 0; t < params.thread; ++t) {
                if (t == params.thread-1) {
                    assigned = n1 - total;
                }

                tbeg[t] = total;
                tend[t] = total+assigned;

                pool[t].start([&iter, &work, &vres, t, tbeg, tend, params, n2]() {
                    for (uint_t i = tbeg[t]; i < tend[t]; ++i) {
                        for (uint_t j = 0; j < n2; ++j) {
                            if (params.self && i == j) continue;
                            work(i,j,vres[t]);
                        }

                        ++iter;
                    }
                });

                total += assigned;
            }

            // Wait for the computation to finish.
            // Here the main thread does nothing except sleeping, occasionally waking up
            // once in a while to update the progress bar if any.
            auto p = progress_start(n1);
            while (iter < n1) {
                thread::sleep_for(0.2);
                if (params.verbose) print_progress(p, iter);
            }

            // By now, all threads should have ended their tasks.
            // We must ask them to terminate nicely.
            for (auto& t : pool) {
                t.join();
            }

            // Merge back the results of each thread
            for (uint_t t = 0; t < params.thread; ++t) {
                auto ids = rgen(tend[t] - tbeg[t]) + tbeg[t];
                res.id(_,ids) = vres[t].id(_,ids);
                res.d(_,ids) = vres[t].d(_,ids);

                if (t == 0) {
                    res.rid = vres[t].rid;
                    res.rd = vres[t].rd;
                } else {
                    for (uint_t j = 0; j < n2; ++j) {
                        if (res.rd[j] >= vres[t].rd[j]) {
                            res.rid[j] = vres[t].rid[j];
                            res.rd[j] = vres[t].rd[j];
                        }
                    }
                }
            }
        }
    }

    // Convert the distance estimator to a real distance
    res.d = 3600.0*(180.0/3.14159265359)*2*asin(sqrt(res.d));
    res.rd = 3600.0*(180.0/3.14159265359)*2*asin(sqrt(res.rd));

    return res;
}

struct id_pair {
    vec1u id1, id2;
    vec1u lost;
};

id_pair xmatch_clean_best(const qxmatch_res& r) {
    const uint_t ngal = dim(r.id)[1];

    id_pair c;
    c.id1.data.reserve(ngal);
    c.id2.data.reserve(ngal);
    c.lost.data.reserve(ngal/6);

    for (uint_t i = 0; i < ngal; ++i) {
        if (r.rid[r.id[i]] == i) {
            c.id1.data.push_back(i);
            c.id2.data.push_back(r.id[i]);
        } else {
            c.lost.data.push_back(i);
        }
    }

    c.id1.dims[0] = c.id1.data.size();
    c.id2.dims[0] = c.id2.data.size();
    c.lost.dims[0] = c.lost.data.size();

    return c;
}

void xmatch_check_lost(const id_pair& p) {
    if (n_elements(p.lost) != 0) {
        warning(n_elements(p.lost), " sources failed to cross match");
    }
}

void xmatch_save_lost(const id_pair& p, const std::string& save) {
    if (n_elements(p.lost) != 0) {
        warning(n_elements(p.lost), " sources failed to cross match");
        fits::write_table(save, ftable(p.lost));
    }
}

#endif
