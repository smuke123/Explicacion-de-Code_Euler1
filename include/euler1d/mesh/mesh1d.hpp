#pragma once

#include <vector>

namespace euler1d {

/// Malla cartesiana uniforme de volumenes finitos.
///
/// Centros de celda: x_k = xmin + (k - 1/2) dx, con k = 1..N (indice Fortran).
/// Caras: x_{k+1/2} = xmin + k dx, con k = 0..N; el flujo numerico Fnum[k]
/// vive en la cara entre las celdas k y k+1.
class Mesh1D {
public:
    Mesh1D(double x_min, double x_max, int cell_count)
        : x_min_(x_min),
          x_max_(x_max),
          cell_count_(cell_count),
          dx_((x_max - x_min) / cell_count) {
        centers_.resize(static_cast<std::size_t>(cell_count));
        for (int k = 1; k <= cell_count; ++k) {
            centers_[static_cast<std::size_t>(k - 1)] = x_min + (k - 0.5) * dx_;
        }
    }

    int cellCount() const { return cell_count_; }
    double dx() const { return dx_; }
    double xMin() const { return x_min_; }
    double xMax() const { return x_max_; }

    /// Centro de la celda fisica k (k = 1..N).
    double center(int k) const { return centers_[static_cast<std::size_t>(k - 1)]; }

    /// Posicion de la cara k+1/2 (k = 0..N).
    double face(int k) const { return x_min_ + k * dx_; }

    const std::vector<double>& centers() const { return centers_; }

private:
    double x_min_;
    double x_max_;
    int cell_count_;
    double dx_;
    std::vector<double> centers_;
};

}  // namespace euler1d
