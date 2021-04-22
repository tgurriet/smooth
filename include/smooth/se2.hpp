#ifndef SMOOTH__SE2_HPP_
#define SMOOTH__SE2_HPP_

#include <random>

#include "common.hpp"
#include "concepts.hpp"
#include "lie_group_base.hpp"
#include "so2.hpp"
#include "macro.hpp"

namespace smooth
{

/**
 * @brief SE2 Lie Group
 *
 * Memory layout
 * =============
 * Group:    x y qz qw
 * Tangent:  vx vy wz
 *
 * Constraints
 * ===========
 * Group:   qz * qz + qw * qw = 1
 * Tangent: -pi < wz <= pi
 */
template<typename _Scalar, StorageLike _Storage = DefaultStorage<_Scalar, 4>>
requires(_Storage::SizeAtCompileTime == 4 && std::is_same_v<typename _Storage::Scalar, _Scalar>)
class SE2 : public LieGroupBase<SE2<_Scalar, _Storage>, 4>
{
private:
  _Storage s_;

public:
  // REQUIRED CONSTANTS

  static constexpr uint32_t lie_size = 4;
  static constexpr uint32_t lie_dof = 3;
  static constexpr uint32_t lie_dim = 3;
  static constexpr uint32_t lie_actdim = 2;

  // CONSTRUCTORS AND OPERATORS

  SMOOTH_GROUP_BOILERPLATE(SE2)

  // SE2-SPECIFIC API

  /**
   * @brief Construct from SO2 and translation
   */
  template<typename Derived>
  SE2(const SO2<Scalar> & so2, const Eigen::MatrixBase<Derived> & translation)
  requires ModifiableStorageLike<Storage>
  {
    s_[0] = translation(0);
    s_[1] = translation(1);
    s_[2] = so2.coeffs()[0];
    s_[3] = so2.coeffs()[1];
  }

  /**
   * @brief Access const SO2 part
   */
  Map<const SO2<Scalar>> so2() const
  requires ModifiableStorageLike<Storage>
  {
    return Map<const SO2<Scalar>>(s_.data() + 2);
  }

  /**
   * @brief Access SO2 part
   */
  Map<SO2<Scalar>> so2()
  requires ModifiableStorageLike<Storage>
  {
    return Map<SO2<Scalar>>(s_.data() + 2);
  }

  /**
   * @brief Access SO2 part by copy
   */
  SO2<Scalar> so2() const
  requires ConstStorageLike<Storage>
  {
    return SO2<Scalar>(s_[2], s_[3]);
  }

  /**
   * @brief Access const E2 part
   */
  Eigen::Map<const Eigen::Matrix<Scalar, 2, 1>> translation() const
  requires ModifiableStorageLike<Storage>
  {
    return Eigen::Map<const Eigen::Matrix<Scalar, 2, 1>>(s_.data());
  }

  /**
   * @brief Access E2 part
   */
  Eigen::Map<Eigen::Matrix<Scalar, 2, 1>> translation()
  requires ModifiableStorageLike<Storage>
  {
    return Eigen::Map<Eigen::Matrix<Scalar, 2, 1>>(s_.data());
  }

  /**
   * @brief Access E2 part by copy
   */
  Eigen::Matrix<Scalar, 2, 1> translation() const
  requires ConstStorageLike<Storage>
  {
    return Eigen::Matrix<Scalar, 2, 1>(s_[0], s_[1]);
  }

  // REQUIRED GROUP API

  /**
   * @brief Set to identity element
   */
  void setIdentity() requires ModifiableStorageLike<Storage>
  {
    s_[0] = Scalar(0); s_[1] = Scalar(0); s_[2] = Scalar(0); s_[3] = Scalar(1);
  }

  /**
   * @brief Set to a random element
   */
  template<typename RNG>
  void setRandom(RNG & rng)
  requires ModifiableStorageLike<Storage>&& std::is_floating_point_v<Scalar>
  {
    const Scalar x = filler<Scalar>(rng, 0);
    const Scalar y = filler<Scalar>(rng, 0);
    const Scalar u = Scalar(2 * M_PI) * filler<Scalar>(rng, 0);

    // x y qz qw
    s_[0] = x;  s_[1] = y; s_[2] = sin(u); s_[3] = cos(u);
  }

  /**
   * @brief Matrix lie group element
   */
  MatrixGroup matrix_group() const
  {
    MatrixGroup ret;
    ret.setIdentity();
    ret.template topLeftCorner<2, 2>() = so2().matrix_group();
    ret.template topRightCorner<2, 1>() = translation();
    return ret;
  }

  /**
   * @brief Group action
   */
  template<typename Derived>
  requires(Derived::IsVectorAtCompileTime == 1 && Derived::SizeAtCompileTime == lie_actdim)
  Vector operator*(const Eigen::MatrixBase<Derived> & x) const
  {
    return so2() * x + translation();
  }

  /**
   * @brief Group composition
   */
  template<typename OS>
  Group operator*(const SE2<Scalar, OS> & r) const
  {
    return Group(so2() * r.so2(), so2() * r.translation() + translation());
  }

  /**
   * @brief Group inverse
   */
  Group inverse() const
  {
    return Group(so2().inverse(), -(so2().inverse() * translation()));
  }

  /**
   * @brief Group logarithm
   */
  Tangent log() const
  {
    using std::abs, std::tan;
    const Scalar th = so2().log()(0);
    const Scalar th2 = th * th;

    const Scalar B = th / Scalar(2);
    Scalar A;
    if (th2 < Scalar(eps2)) {
      // https://www.wolframalpha.com/input/?i=series+x+%2F+tan+x+at+x%3D0
      A = Scalar(1) - th2 / Scalar(12);
    } else {
      A = B / std::tan(B);
    }

    Eigen::Matrix<Scalar, 2, 2> Sinv;
    Sinv(0, 0) = A;
    Sinv(1, 1) = A;
    Sinv(0, 1) = B;
    Sinv(1, 0) = -B;

    Tangent ret;
    ret.template head<2>() = Sinv * translation();
    ret(2) = th;
    return ret;
  }

  /**
   * @brief Group adjoint
   */
  TangentMap Ad() const
  {
    TangentMap ret;
    ret.template topLeftCorner<2, 2>() = so2().matrix_group();
    ret(0, 2) = translation().y();
    ret(1, 2) = -translation().x();
    ret(2, 0) = Scalar(0);
    ret(2, 1) = Scalar(0);
    ret(2, 2) = Scalar(1);
    return ret;
  }

  // REQUIRED TANGENT API

  /**
   * @brief Group exponential
   */
  template<typename Derived>
  static Group exp(const Eigen::MatrixBase<Derived> & a)
  requires(Derived::IsVectorAtCompileTime == 1 && Derived::SizeAtCompileTime == lie_dof)
  {
    using std::abs, std::cos, std::sin;

    const Scalar th = a.z();
    const Scalar th2 = th * th;

    Scalar A, B;
    if (th2 < Scalar(eps2)) {
      // https://www.wolframalpha.com/input/?i=series+sin+x+%2F+x+at+x%3D0
      A = Scalar(1) - th2 / Scalar(6);
      // https://www.wolframalpha.com/input/?i=series+%28cos+x+-+1%29+%2F+x+at+x%3D0
      B = -th / Scalar(2) + th * th2 / Scalar(24);
    } else {
      A = sin(th) / th;
      B = (cos(th) - Scalar(1)) / th;
    }

    Eigen::Matrix<Scalar, 2, 2> S;
    S(0, 0) = A;
    S(1, 1) = A;
    S(0, 1) = B;
    S(1, 0) = -B;

    return Group(
      SO2<Scalar>::exp(a.template tail<1>()),
      S * a.template head<2>()
    );
  }

  /**
   * @brief Algebra adjoint
   */
  template<typename Derived>
  static TangentMap ad(const Eigen::MatrixBase<Derived> & a)
  requires(Derived::IsVectorAtCompileTime == 1 && Derived::SizeAtCompileTime == lie_dof)
  {
    TangentMap ret;
    ret.setZero();
    ret.template topLeftCorner<2, 2>() = SO2<Scalar>::hat(a.template tail<1>());
    ret(0, 2) = a.y();
    ret(1, 2) = -a.x();
    return ret;
  }

  /**
   * @brief Algebra hat
   */
  template<typename Derived>
  static MatrixGroup hat(const Eigen::MatrixBase<Derived> & a)
  requires(Derived::IsVectorAtCompileTime == 1 && Derived::SizeAtCompileTime == lie_dof)
  {
    MatrixGroup ret;
    ret.setZero();
    ret.template topLeftCorner<2, 2>() = SO2<Scalar>::hat(a.template tail<1>());
    ret.template topRightCorner<2, 1>() = a.template head<2>();
    return ret;
  }

  /**
   * @brief Algebra vee
   */
  template<typename Derived>
  static Tangent vee(const Eigen::MatrixBase<Derived> & A)
  requires(Derived::RowsAtCompileTime == lie_dim && Derived::ColsAtCompileTime == lie_dim)
  {
    Tangent t;
    t.template tail<1>() = SO2<Scalar>::vee(A.template topLeftCorner<2, 2>());
    t.template head<2>() = A.template topRightCorner<2, 1>();
    return t;
  }

  /**
   * @brief Right jacobian of the exponential map
   */
  template<typename Derived>
  static TangentMap dr_exp(const Eigen::MatrixBase<Derived> & a)
  requires(Derived::IsVectorAtCompileTime == 1 && Derived::SizeAtCompileTime == lie_dof)
  {
    using std::abs, std::sqrt, std::sin, std::cos;
    const Scalar th2 = a.z() * a.z();

    Scalar A, B;
    if (th2 < Scalar(eps2)) {
      // https://www.wolframalpha.com/input/?i=series+%281-cos+x%29+%2F+x%5E2+at+x%3D0
      A = Scalar(1) / Scalar(2) - th2 / Scalar(24);
      // https://www.wolframalpha.com/input/?i=series+%28x+-+sin%28x%29%29+%2F+x%5E3+at+x%3D0
      B = Scalar(1) / Scalar(6) - th2 / Scalar(120);
    } else {
      const Scalar th = a.z();
      A = (Scalar(1) - cos(th)) / th2;
      B = (th - sin(th)) / (th2 * th);
    }
    const TangentMap ad = SE2<Scalar>::ad(a);
    return TangentMap::Identity() - A * ad + B * ad * ad;
  }

  /**
   * @brief Inverse of the right jacobian of the exponential map
   */
  template<typename Derived>
  static TangentMap dr_expinv(const Eigen::MatrixBase<Derived> & a)
  requires(Derived::IsVectorAtCompileTime == 1 && Derived::SizeAtCompileTime == lie_dof)
  {
    using std::abs, std::sqrt, std::sin, std::cos;
    const Scalar th = a.z();
    const Scalar th2 = th * th;
    const TangentMap ad = SE2<Scalar>::ad(a);

    Scalar A;
    if (th2 < Scalar(eps2)) {
      // https://www.wolframalpha.com/input/?i=series+1%2Fx%5E2+-+%281+%2B+cos+x%29+%2F+%282+*+x+*+sin+x%29+at+x%3D0
      A = Scalar(1) / Scalar(12) + th2 / Scalar(720);
    } else {
      A = (Scalar(1) / th2) - (Scalar(1) + cos(th)) / (Scalar(2) * th * sin(th));
    }
    return TangentMap::Identity() + ad / 2 + A * ad * ad;
  }
};

using SE2f = SE2<float>;
using SE2d = SE2<double>;

}  // namespace smooth

#endif  // SMOOTH__SE2_HPP_
