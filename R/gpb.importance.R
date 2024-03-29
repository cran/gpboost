#' @name gpb.importance
#' @title Compute feature importance in a model
#' @description Creates a \code{data.table} of feature importances in a model.
#' @param model object of class \code{gpb.Booster}.
#' @param percentage whether to show importance in relative percentage.
#'
#' @return For a tree model, a \code{data.table} with the following columns:
#' \itemize{
#'   \item{\code{Feature}: Feature names in the model.}
#'   \item{\code{Gain}: The total gain of this feature's splits.}
#'   \item{\code{Cover}: The number of observation related to this feature.}
#'   \item{\code{Frequency}: The number of times a feature splited in trees.}
#' }
#'
#' @examples
#' \donttest{
#' data(agaricus.train, package = "gpboost")
#' train <- agaricus.train
#' dtrain <- gpb.Dataset(train$data, label = train$label)
#'
#' params <- list(
#'   objective = "binary"
#'   , learning_rate = 0.1
#'   , max_depth = -1L
#'   , min_data_in_leaf = 1L
#'   , min_sum_hessian_in_leaf = 1.0
#' )
#' model <- gpb.train(
#'     params = params
#'     , data = dtrain
#'     , nrounds = 5L
#' )
#'
#' tree_imp1 <- gpb.importance(model, percentage = TRUE)
#' tree_imp2 <- gpb.importance(model, percentage = FALSE)
#' }
#' @importFrom data.table := setnames setorderv
#' @export
gpb.importance <- function(model, percentage = TRUE) {

  # Check if model is a gpboost model
  if (!gpb.is.Booster(x = model)) {
    stop("'model' has to be an object of class gpb.Booster")
  }

  # Setup importance
  tree_dt <- gpb.model.dt.tree(model = model)

  # Extract elements
  tree_imp_dt <- tree_dt[
    !is.na(split_index)
    , .(Gain = sum(split_gain), Cover = sum(internal_count), Frequency = .N)
    , by = "split_feature"
  ]

  data.table::setnames(
    x = tree_imp_dt
    , old = "split_feature"
    , new = "Feature"
  )

  # Sort features by Gain
  data.table::setorderv(
    x = tree_imp_dt
    , cols = "Gain"
    , order = -1L
  )

  # Check if relative values are requested
  if (percentage) {
    tree_imp_dt[, `:=`(
      Gain = Gain / sum(Gain)
      , Cover = Cover / sum(Cover)
      , Frequency = Frequency / sum(Frequency)
    )]
  }

  return(tree_imp_dt[])

}
