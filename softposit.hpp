#ifndef uu_recreate_wheel_imp_SOFTPOSIT_H
#define uu_recreate_wheel_imp_SOFTPOSIT_H

#include <armadillo>
#include <boost/optional.hpp>

void softposit(float* rot, float* trans, int* foundPose, int* imagePts, float* worldPts, int* nbImagePts, int* nbWorldPts, float* beta0, float* noiseStd, float* initRot, float* initTrans, float* focalLength, int* center){


}


#include <tuple>
#include <vector>
#include <utility>

using vec3_type = arma::vec3;
using mat3_type = arma::mat33;

using point2di_type = arma::ivec2;
using point2d_type = arma::vec2;
using point3d_type = arma::vec3;

using rodrigue_type = vec3_type;

using match_type = std::vector<std::pair<int, int> >;


struct Param_type{
  double beta0;
  double noiseStd;
};


struct Pose_type{
  mat3_type rot;
  vec3_type trans;
};

struct Image_type{
  float focalLength;
  point2di_type center;
};


namespace my{
  template<typename Dst_container_t, typename Src_container_t>
  Dst_container_t transform(const Src_container_t& _src, std::function<typename Dst_container_t::value_type (typename Src_container_t::const_reference)> F){
    Dst_container_t dst(_src.size()); // todo: to do tricks;
    std::transform(_src.begin(), _src.end(), dst.begin(), F);
    return std::move(dst);
  }
}



namespace imp{
  using stat_info = arma::vec5;

}

arma::mat sinkhornSlack(arma::mat M);

int numMatches(arma::mat assignMat);

// at least it will find something
boost::optional< std::tuple<Pose_type, match_type> > softposit(
  const std::vector<point2di_type>& imagePts,
  const std::vector<point3d_type>& worldPts,
  const Param_type& param,
  const Pose_type& initpose,
  boost::optional<const Image_type&> maybe_caminfo){

  using namespace std;

  std::vector<imp::stat_info> stats;

  auto alpha = 9.21*pow(param.noiseStd,2) + 1;
  auto maxDelta = sqrt(alpha)/2;          //  Max allowed error per world point.

  auto betaFinal = 0.5;                  // Terminate iteration when beta == betaFinal.
  auto betaUpdate = 1.05;                // Update rate on beta.
  auto epsilon0 = 0.01;                  // Used to initialize assignement matrix.

  auto maxCount = 2;
  auto minBetaCount = 20;
  auto nbImagePts = imagePts.size();
  auto nbWorldPts = worldPts.size();

  auto minNbPts = std::min(nbImagePts, nbWorldPts);
  auto maxNbPts = nbImagePts+nbWorldPts - minNbPts;

  auto scale = 1.0/(maxNbPts + 1);

  std::vector<point2d_type> _centeredImage(imagePts.size());

  Image_type caminfo;
  if (maybe_caminfo)
    caminfo = *maybe_caminfo;
  else{
    caminfo.focalLength = 1;
    caminfo.center = point2di_type{0, 0};
  }

  std::transform(imagePts.begin(), imagePts.end(),
                 _centeredImage.begin(),
                 [&caminfo](const point2di_type & _pt){
                   return point2d_type((point2d_type{double(_pt[0]), double(_pt[1])} - caminfo.center)/caminfo.focalLength);
                 });;

  arma::mat centeredImage = arma::zeros<arma::mat>(_centeredImage.size(),2);
  for (int j=0; j<centeredImage.n_cols; ++j)
  {
    for (int i=0; i<centeredImage.n_rows; ++i){
      centeredImage(i,j) = _centeredImage[i][j];
    }
  }

  auto homogeneousWorldPts = my::transform<std::vector<arma::vec4>>(worldPts, [](const auto & _pt){return arma::vec4{_pt[0], _pt[1], _pt[2], 1}; });


  auto pose = initpose;

  auto wk = arma::vec(my::transform<std::vector<double>>(homogeneousWorldPts,
                                                         [&pose](const auto& _pt)->double{
                                                           return arma::dot(_pt,  arma::vec4 {pose.rot(2,0), pose.rot(2,1), pose.rot(2,2),1});
                                                         }));
  arma::vec4 r1T = {pose.rot(0,0)/pose.trans(2), pose.rot(0,1)/pose.trans(2), pose.rot(0,2)/pose.trans(2), pose.trans(0)/pose.trans(2)};
  arma::vec4 r2T = {pose.rot(1,0)/pose.trans(2), pose.rot(1,1)/pose.trans(2), pose.rot(1,2)/pose.trans(2), pose.trans(1)/pose.trans(2)};

  auto  betaCount = 0;
  auto poseConverged = 0;
  auto assignConverged = false;
  auto foundPose = 0;
  auto beta = param.beta0;

  arma::mat assignMat = arma::ones(nbImagePts+1,nbWorldPts+1) + epsilon0;

  auto imageOnes = arma::ones<arma::mat>(nbImagePts, 1);

  while (beta < betaFinal && !assignConverged)
  {
    auto projectedU = arma::vec(my::transform<std::vector<double>>(homogeneousWorldPts, [&r1T](const auto& _p)->double{ return arma::dot(_p, r1T);}));          // Column N-vector.

    auto projectedV = arma::vec(my::transform<std::vector<double>>(homogeneousWorldPts, [&r2T](const auto& _p)->double{ return arma::dot(_p, r2T);}));          // Column N-vector.


    auto replicatedProjectedU = imageOnes * projectedU.t();
    auto replicatedProjectedV = imageOnes * projectedV.t();

    auto wkxj = centeredImage(0) * wk.t();
    auto wkyj = centeredImage(1) * wk.t();

    auto distMat = caminfo.focalLength*caminfo.focalLength*(arma::square(replicatedProjectedU - wkxj) + arma::square (replicatedProjectedV - wkyj));

    assignMat(arma::span(0, nbImagePts-1), arma::span(0, nbWorldPts-1)) = scale*arma::exp(-beta*(distMat - alpha));
    assignMat.col(nbWorldPts) = scale;
    assignMat.row(nbImagePts) = scale;


    assignMat = sinkhornSlack (assignMat);    // My "improved" Sinkhorn.
    numMatchPts = numMatches(assignMat);
    sumNonslack = arma::sum(assignMat(0,0,nbImagePts-1,nbWorldPts-1));

    auto summedByColAssign = arma::sum(assignMat(0, 0, nbImagePts-1, nbWorldPts-1)).eval();
    arma::mat sumSkSkT = amra::zeros<arma::mat>(4, 4);
    for(auto  k = 0; k<nbWorldPts; ++k){
      sumSkSkT = sumSkSkT + summedByColAssign(k) * homogeneousWorldPts.row(k).t() * homogeneousWorldPts.row(k);
    }
    if cond(sumSkSkT) > 1e10
             std::cout<<('sumSkSkT is ill-conditioned, termininating search.')<<std::endl;
    return boost::none;

    arma::mat objectMat = arma::inv(sumSkSkT);                           // Inv(L), a 4x4 matrix.
    poseConverged = 0;                              // Initialize for POSIT loop.
    count = 0;

    // Save the previouse pose vectors for convergence checks.
    auto r1Tprev = r1T;
    auto r2Tprev = r2T;

    arma::vec weightedUi(4, 0) ;
    arma::vec weightedVi(4, 0) ;
    for (int j=0;j<nbImagePts; ++j){
      for (int k=0; k<nbWorldPts; ++k){
        weightedUi = weightedUi + assignMat(j,k) * wk(k) * centeredImage(j,0) * homogeneousWorldPts(k,arma::span::all).t();
        weightedVi = weightedVi + assignMat(j,k) * wk(k) * centeredImage(j,1) * homogeneousWorldPts(k,arma::span::all).t();
      }
    }

    r1T= objectMat * weightedUi;
    r2T = objectMat * weightedVi;

    arma::mat U, V;
    arma::vec s;
    arma::mat X(3,2);
    X(0) = r1T(arma::span(0,2));
    X(1) = r2T(arma::span(0,2));

    arma::svd(U,s,V, X);

  arma:mat A = U * arma::mat("1 0; 0 1; 0 0") * V.t();

    auto r1 = A(0);
    auto r2 = A(1);
    arma::vec r3 = arma::cross(r1,r2);
    double Tz = 2 / (S(0) + S(1));
    auto Tx = r1T(3) * Tz;
    auto Ty = r2T(3) * Tz;
    auto r3T= arma::vec{r3[0], r3[1], r3[2], Tz};

    auto r1T = arma::vec{r1[0], r1[1], r1[2], Tx}/Tz;
    auto r2T = arma::vec{r2[0], r2[1], r2[3], Ty}/Tz;


    wk = homogeneousWorldPts * r3T /Tz;

    double delta = arma::sqrt(arma::sum(assignMat(0, 0, nbImagePts-1, nbWorldPts-1) % distMat)/nbWorldPts);
    poseConverged = delta < maxDelta;

    stats(betaCount,0) = beta;
    stats(betaCount,1) = delta;
    stats(betaCount,2) = numMatchPts/nbWorldPts;
    stats(betaCount,3) = sumNonslack/nbWorldPts;
    stats(betaCount,4) = arma::sum(arma::square(r1T-r1Tprev)) + arma::sum(arma::square(r2T-r2Tprev));

    count = count + 1;

    beta = betaUpdate * beta;
    betaCount = betaCount + 1;
    assignConverged = poseConverged && betaCount > minBetaCount;

    pose.trans = arma::vec{Tx, Ty, Tz}
    pose.rot.row(0) = r1.t();
    pose.rot.row(1) = r2.t();
    pose.rot.row(2) = r3.t();

    foundPose = (delta < maxDelta && betaCount > minBetaCount);
 }


  return make_tuple(initpose, match_type());
}


arma::mat sinkhornSlack(arma::mat M)
{
  arma::mat normalizedMat ;
  auto iMaxIterSinkhorn=60;  // In PAMI paper
  auto fEpsilon2 = 0.001; // Used in termination of Sinkhorn Loop.

  auto iNumSinkIter = 0;
  int nbRows = M.n_rows;
  int nbCols = M.n_cols;

  auto fMdiffSum = fEpsilon2 + 1;

  while(abs(fMdiffSum) > fEpsilon2 && iNumSinkIter < iMaxIterSinkhorn){
    auto Mprev = M; // % Save M from previous iteration to test for loop termination

   // Col normalization (except outlier row - do not normalize col slacks against each other)
    auto McolSums = arma::sum(M); // Row vector.
    McolSums(nbCols-1) = 1; // Don't normalize slack col terms against each other.
    auto McolSumsRep = arma::ones<arma::vec>(nbRows) * McolSums ;
    M = M / McolSumsRep;

    // Row normalization (except outlier row - do not normalize col slacks against each other)
    auto MrowSums = arma::sum(M, 1); // Column vector.
    MrowSums(nbRows-1) = 1; // Don't normalize slack row terms against each other.
    auto MrowSumsRep = MrowSums * arma::ones<arma::rowvec>(nbCols);
    M = M ./ MrowSumsRep;

    iNumSinkIter=iNumSinkIter+1;
    fMdiffSum=arma::sum(arma::abs(M-Mprev));
  }

  normalizedMat = M;

  return normalizedMat;
}




int numMatches(arma::mat assignMat)
{
  int num = 0;

  auto nimgpnts  = assignMat.n_rows - 1;
  auto nmodpnts = assignMat.n_cols - 1;

  for (int k = 0; k< nmodpnts; ++k){

    int imax;
    auto vmax = assignMat.cols(k).max(imax);

    if(imax == assignMat.n_rows - 1) continue; // Slack value is maximum in this column.


    std::vector<int> other_cols;
    other_cols.reserve(assignMat.n_cols-1);
    for (int i=0; i<assignMat.n_cols; ++i)
      if (i!=k) other_cols.push_back(i);
    if (arma::all(vmax > assignMat(imax,other_cols)))
            num = num + 1;              // This value is maximal in its row & column.
  }
  return num;
}



#endif /* uu_recreate_wheel_imp_SOFTPOSIT_H */