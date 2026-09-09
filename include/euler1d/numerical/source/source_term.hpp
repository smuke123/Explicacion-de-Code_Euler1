#pragma once

#include <string>
#include <vector>

#include "euler1d/mesh/mesh1d.hpp"
#include "euler1d/physics/field.hpp"
#include "euler1d/physics/state.hpp"

namespace euler1d {

/// Ensambla la actualizacion conservativa a partir de los flujos de cara.
///
/// Se separa de los terminos fuente puros porque la geometria NO es una
/// fuente aditiva: cambia la propia divergencia (areas y volumenes de las
/// caras). El Fortran mezclaba ambas cosas en `rhs_fv`.
class DivergenceOperator {
public:
    virtual ~DivergenceOperator() = default;

    /// u_out[k] = u_in[k] - dt * div(F)[k], solo en celdas fisicas 1..N.
    virtual void apply(const ConservedField& u_in,
                       const PrimitiveField& w,
                       const std::vector<FluxVector>& fluxes,
                       double dt,
                       ConservedField& u_out) const = 0;

    virtual std::string name() const = 0;
};

/// Geometria plana (alpha = 0): divergencia cartesiana (F_{k+1/2} - F_{k-1/2})/dx.
class CartesianDivergence final : public DivergenceOperator {
public:
    explicit CartesianDivergence(double dx) : dx_(dx) {}

    void apply(const ConservedField& u_in,
               const PrimitiveField& w,
               const std::vector<FluxVector>& fluxes,
               double dt,
               ConservedField& u_out) const override;

    std::string name() const override { return "cartesiana"; }

private:
    double dx_;
};

/// Geometria cilindrica (alpha = 1) o esferica (alpha = 2) en forma
/// CONSERVATIVA: areas A = r^alpha en las caras y volumen exacto
///     V = (r_+^{alpha+1} - r_-^{alpha+1}) / (alpha+1),
/// mas el termino geometrico de presion S_p = p_k (A_+ - A_-)/V, que cancela
/// exactamente la divergencia de una presion uniforme.
///
/// La forma alternativa -(alpha/r) F NO conserva: segun la auditoria del
/// Fortran, el arranque del Sedov inyectaba un 42% de energia espuria.
class RadialDivergence final : public DivergenceOperator {
public:
    RadialDivergence(const Mesh1D& mesh, int alpha) : mesh_(&mesh), alpha_(alpha) {}

    void apply(const ConservedField& u_in,
               const PrimitiveField& w,
               const std::vector<FluxVector>& fluxes,
               double dt,
               ConservedField& u_out) const override;

    std::string name() const override { return alpha_ == 1 ? "cilindrica" : "esferica"; }

private:
    const Mesh1D* mesh_;
    int alpha_;
};

/// Termino fuente aditivo: se acumula sobre una actualizacion ya ensamblada.
class SourceTerm {
public:
    virtual ~SourceTerm() = default;

    virtual void accumulate(const PrimitiveField& w, double dt, ConservedField& u_out) const = 0;

    virtual std::string name() const = 0;
};

/// Gravedad constante en +x: S = (0, -rho g, -rho g u).
///
/// El trabajo de la gravedad usa la velocidad del INICIO del subpaso
/// (`W(2,k)`), no la actualizada. Se conserva tal cual.
class GravitySource final : public SourceTerm {
public:
    explicit GravitySource(double gravity) : gravity_(gravity) {}

    void accumulate(const PrimitiveField& w, double dt, ConservedField& u_out) const override;

    std::string name() const override { return "gravedad"; }

private:
    double gravity_;
};

}  // namespace euler1d
