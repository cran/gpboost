% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/GPModel.R
\name{summary.GPModel}
\alias{summary.GPModel}
\title{Summary for a \code{GPModel}}
\usage{
\method{summary}{GPModel}(object, ...)
}
\arguments{
\item{object}{a \code{GPModel}}

\item{...}{(not used, ignore this, simply here that there is no CRAN warning)}
}
\value{
Summary of a (fitted) \code{GPModel}
}
\description{
Summary for a \code{GPModel}
}
\examples{
# See https://github.com/fabsig/GPBoost/tree/master/R-package for more examples

data(GPBoost_data, package = "gpboost")
# Add intercept column
X1 <- cbind(rep(1,dim(X)[1]),X)
X_test1 <- cbind(rep(1,dim(X_test)[1]),X_test)

#--------------------Grouped random effects model: single-level random effect----------------
gp_model <- fitGPModel(group_data = group_data[,1], y = y, X = X1,
                       likelihood="gaussian", params = list(std_dev = TRUE))
summary(gp_model)


\donttest{
#--------------------Gaussian process model----------------
gp_model <- fitGPModel(gp_coords = coords, cov_function = "matern", cov_fct_shape = 1.5,
                       likelihood="gaussian", y = y, X = X1, params = list(std_dev = TRUE))
summary(gp_model)
}

}
\author{
Fabio Sigrist
}
