#' @name gpb.model.dt.tree
#' @title Parse a GPBoost model json dump
#' @description Parse a GPBoost model json dump into a \code{data.table} structure.
#' @param model object of class \code{gpb.Booster}
#' @param num_iteration number of iterations you want to predict with. NULL or
#'                      <= 0 means use best iteration
#' @return
#' A \code{data.table} with detailed information about model trees' nodes and leafs.
#'
#' The columns of the \code{data.table} are:
#'
#' \itemize{
#'  \item{\code{tree_index}: ID of a tree in a model (integer)}
#'  \item{\code{split_index}: ID of a node in a tree (integer)}
#'  \item{\code{split_feature}: for a node, it's a feature name (character);
#'                              for a leaf, it simply labels it as \code{"NA"}}
#'  \item{\code{node_parent}: ID of the parent node for current node (integer)}
#'  \item{\code{leaf_index}: ID of a leaf in a tree (integer)}
#'  \item{\code{leaf_parent}: ID of the parent node for current leaf (integer)}
#'  \item{\code{split_gain}: Split gain of a node}
#'  \item{\code{threshold}: Splitting threshold value of a node}
#'  \item{\code{decision_type}: Decision type of a node}
#'  \item{\code{default_left}: Determine how to handle NA value, TRUE -> Left, FALSE -> Right}
#'  \item{\code{internal_value}: Node value}
#'  \item{\code{internal_count}: The number of observation collected by a node}
#'  \item{\code{leaf_value}: Leaf value}
#'  \item{\code{leaf_count}: The number of observation collected by a leaf}
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
#'   , learning_rate = 0.01
#'   , num_leaves = 63L
#'   , max_depth = -1L
#'   , min_data_in_leaf = 1L
#'   , min_sum_hessian_in_leaf = 1.0
#' )
#' model <- gpb.train(params, dtrain, 10L)
#'
#' tree_dt <- gpb.model.dt.tree(model)
#' }
#' @importFrom data.table := rbindlist
#' @importFrom RJSONIO fromJSON
#' @export
gpb.model.dt.tree <- function(model, num_iteration = NULL) {

  # Dump json model first
  json_model <- gpb.dump(booster = model, num_iteration = num_iteration)

  # Parse json model second
  parsed_json_model <- RJSONIO::fromJSON(content = json_model, simplify = FALSE)
  # Make sure that format is correct
  if (is.list(parsed_json_model[[9]])) {
    parsed_json_model[[9]] <- unlist(parsed_json_model[[9]])
  }

  # Parse tree model third
  tree_list <- lapply(parsed_json_model$tree_info, single.tree.parse)

  # Combine into single data.table fourth
  tree_dt <- data.table::rbindlist(l = tree_list, use.names = TRUE)

  # Substitute feature index with the actual feature name

  # Since the index comes from C++ (which is 0-indexed), be sure
  # to add 1 (e.g. index 28 means the 29th feature in feature_names)
  split_feature_indx <- tree_dt[, split_feature] + 1L

  # Get corresponding feature names. Positions in split_feature_indx
  # which are NA will result in an NA feature name
  feature_names <- parsed_json_model$feature_names[split_feature_indx]
  tree_dt[, split_feature := feature_names]

  return(tree_dt)

}


#' @importFrom data.table := data.table rbindlist
single.tree.parse <- function(lgb_tree) {

  # Traverse tree function
  pre_order_traversal <- function(env = NULL, tree_node_leaf, current_depth = 0L, parent_index = NA_integer_) {

    if (is.null(env)) {
      # Setup initial default data.table with default types
      env <- new.env(parent = emptyenv())
      env$single_tree_dt <- data.table::data.table(
        tree_index = integer(0L)
        , depth = integer(0L)
        , split_index = integer(0L)
        , split_feature = integer(0L)
        , node_parent = integer(0L)
        , leaf_index = integer(0L)
        , leaf_parent = integer(0L)
        , split_gain = numeric(0L)
        , threshold = numeric(0L)
        , decision_type = character(0L)
        , default_left = character(0L)
        , internal_value = integer(0L)
        , internal_count = integer(0L)
        , leaf_value = integer(0L)
        , leaf_count = integer(0L)
      )
      # start tree traversal
      pre_order_traversal(
        env = env
        , tree_node_leaf = tree_node_leaf
        , current_depth = current_depth
        , parent_index = parent_index
      )
    } else {

      # Check if split index is not null in leaf
      if (!is.null(tree_node_leaf$split_index)) {

        # update data.table
        env$single_tree_dt <- data.table::rbindlist(l = list(env$single_tree_dt,
                                                             c(tree_node_leaf[c("split_index",
                                                                                "split_feature",
                                                                                "split_gain",
                                                                                "threshold",
                                                                                "decision_type",
                                                                                "default_left",
                                                                                "internal_value",
                                                                                "internal_count")],
                                                               "depth" = current_depth,
                                                               "node_parent" = parent_index)),
                                                    use.names = TRUE,
                                                    fill = TRUE)

        # Traverse tree again both left and right
        pre_order_traversal(
          env = env
          , tree_node_leaf = tree_node_leaf$left_child
          , current_depth = current_depth + 1L
          , parent_index = tree_node_leaf$split_index
        )
        pre_order_traversal(
          env = env
          , tree_node_leaf = tree_node_leaf$right_child
          , current_depth = current_depth + 1L
          , parent_index = tree_node_leaf$split_index
        )

      } else if (!is.null(tree_node_leaf$leaf_index)) {

        # update data.table
        env$single_tree_dt <- data.table::rbindlist(l = list(env$single_tree_dt,
                                                             c(tree_node_leaf[c("leaf_index",
                                                                                "leaf_value",
                                                                                "leaf_count")],
                                                               "depth" = current_depth,
                                                               "leaf_parent" = parent_index)),
                                                    use.names = TRUE,
                                                    fill = TRUE)

      }

    }
    return(env$single_tree_dt)
  }

  # Traverse structure
  single_tree_dt <- pre_order_traversal(tree_node_leaf = lgb_tree$tree_structure)

  # Store index
  single_tree_dt[, tree_index := lgb_tree$tree_index]

  return(single_tree_dt)

}
