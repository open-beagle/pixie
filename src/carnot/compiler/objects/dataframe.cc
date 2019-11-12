#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/objects/none_object.h"

namespace pl {
namespace carnot {
namespace compiler {
Dataframe::Dataframe(OperatorIR* op) : QLObject(DataframeType, op), op_(op) {
  CHECK(op != nullptr) << "Bad argument in Dataframe constructor.";
  // TODO(philkuz) (PL-1128) re-enable this when new join syntax is supported.
  /**
   * # Equivalent to the python method method syntax:
   * def merge(self, right, how, left_on, right_on, suffixes=('_x', '_y')):
   *     ...
   */
  // TODO(philkuz) If there's a chance convert the internals of FuncObject to compile time
  // checking of the default arguments. Everytime we create a Dataframe object you have to make this
  // binding.
  // std::shared_ptr<FuncObject> mergefn(new FuncObject(
  //     kMergeOpId, {"right", "how", "left_on", "right_on", "suffixes"},
  //     {{"suffixes", "('_x', '_y')"}},
  //     /* has_kwargs */ false,
  //     std::bind(&JoinHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  // AddMethod(kMergeOpId, mergefn);

  /**
   * # Equivalent to the python method method syntax:
   * def agg(self, **kwargs):
   *     ...
   */
  // TODO(philkuz) (PL-1128) re-enable this when new agg syntax is supported.
  // std::shared_ptr<FuncObject> aggfn(new FuncObject(
  //     kBlockingAggOpId, {}, {},
  //     /* has_kwargs */ true,
  //     std::bind(&AggHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  // AddMethod(kBlockingAggOpId, aggfn);

  /**
   * # Equivalent to the python method method syntax:
   * def range(self, start, stop=plc.now()):
   *     ...
   */
  std::shared_ptr<FuncObject> rangefn(new FuncObject(
      kRangeOpId, {"start", "stop"}, {{"stop", "plc.now()"}},
      /* has_kwargs */ false,
      std::bind(&RangeHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kRangeOpId, rangefn);

  // TODO(philkuz) (PL-1036) remove this upon availability of new syntax.
  /**
   * # Equivalent to the python method method syntax:
   * def map(self, fn):
   *     ...
   */
  std::shared_ptr<FuncObject> mapfn(new FuncObject(
      kMapOpId, {"fn"}, {}, /* has_kwargs */ false,
      std::bind(&OldMapHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kMapOpId, mapfn);

  // TODO(philkuz) (PL-1038) remove this upon availability of new syntax.
  /**
   * # Equivalent to the python method method syntax:
   * def filter(self, fn):
   *     ...
   */
  std::shared_ptr<FuncObject> filterfn(new FuncObject(
      kFilterOpId, {"fn"}, {}, /* has_kwargs */ false,
      std::bind(&OldFilterHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kFilterOpId, filterfn);

  /**
   * # Equivalent to the python method method syntax:
   * def limit(self, rows):
   *     ...
   */
  std::shared_ptr<FuncObject> limitfn(new FuncObject(
      kLimitOpId, {"rows"}, {}, /* has_kwargs */ false,
      std::bind(&LimitHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kLimitOpId, limitfn);

  // TODO(philkuz) (PL-1128) disable this when new agg syntax is supported.
  /**
   * # Equivalent to the python method method syntax:
   * def agg(self, by, fn):
   *     ...
   */
  std::shared_ptr<FuncObject> aggfn(new FuncObject(
      kBlockingAggOpId, {"by", "fn"}, {{"by", "lambda x : []"}},
      /* has_kwargs */ false,
      std::bind(&OldAggHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kBlockingAggOpId, aggfn);
  // TODO(philkuz) (PL-1128) disable this when new join syntax is supported.
  /**
   * # Equivalent to the python method method syntax:
   * def merge(self, right, cond, cols, type="inner"):
   *     ...
   */
  std::shared_ptr<FuncObject> old_join_fn(new FuncObject(
      kMergeOpId, {"right", "cond", "cols", "type"}, {{"type", "'inner'"}},
      /* has_kwargs */ false,
      std::bind(&OldJoinHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kMergeOpId, old_join_fn);

  // TODO(philkuz) (PL-1128) disable this when new result syntax is supported.
  /**
   * # Equivalent to the python method method syntax:
   * def result(self, name):
   *     ...
   */
  std::shared_ptr<FuncObject> old_sink_fn(new FuncObject(
      kSinkOpId, {"name"}, {},
      /* has_kwargs */ false,
      std::bind(&OldResultHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kSinkOpId, old_sink_fn);
}

StatusOr<QLObjectPtr> JoinHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                        const ParsedArgs& args) {
  // GetArg returns non-nullptr or errors out in Debug mode. No need
  // to check again.
  IRNode* right_node = args.GetArg("right");
  IRNode* how_node = args.GetArg("how");
  IRNode* left_on_node = args.GetArg("left_on");
  IRNode* right_on_node = args.GetArg("right_on");
  IRNode* suffixes_node = args.GetArg("suffixes");
  if (!Match(right_node, Operator())) {
    return right_node->CreateIRNodeError("'right' must be an operator, got $0",
                                         right_node->type_string());
  }
  OperatorIR* right = static_cast<OperatorIR*>(right_node);

  if (!Match(how_node, String())) {
    return how_node->CreateIRNodeError("'how' must be a string, got $0", how_node->type_string());
  }
  std::string how_type = static_cast<StringIR*>(how_node)->str();

  PL_ASSIGN_OR_RETURN(std::vector<ColumnIR*> left_on_cols, ProcessCols(left_on_node, "left_on", 0));
  PL_ASSIGN_OR_RETURN(std::vector<ColumnIR*> right_on_cols,
                      ProcessCols(right_on_node, "right_on", 1));

  // TODO(philkuz) consider using a struct instead of a vector because it's a fixed size.
  if (!Match(suffixes_node, ListWithChildren(String()))) {
    return suffixes_node->CreateIRNodeError(
        "'suffixes' must be a tuple with 2 strings - for the left and right suffixes.");
  }

  PL_ASSIGN_OR_RETURN(std::vector<std::string> suffix_strs,
                      ParseStringListIR(static_cast<ListIR*>(suffixes_node)));
  if (suffix_strs.size() != 2) {
    return suffixes_node->CreateIRNodeError(
        "'suffixes' must be a tuple with 2 elements. Received $0", suffix_strs.size());
  }

  PL_ASSIGN_OR_RETURN(JoinIR * join_op, df->graph()->MakeNode<JoinIR>(ast));
  PL_RETURN_IF_ERROR(
      join_op->Init({df->op(), right}, how_type, left_on_cols, right_on_cols, suffix_strs));
  return StatusOr(std::make_shared<Dataframe>(join_op));
}

StatusOr<std::vector<ColumnIR*>> JoinHandler::ProcessCols(IRNode* node, std::string arg_name,
                                                          int64_t parent_index) {
  DCHECK(node != nullptr);
  IR* graph = node->graph_ptr();
  if (Match(node, ListWithChildren(String()))) {
    auto list = static_cast<ListIR*>(node);
    std::vector<ColumnIR*> columns(list->children().size());
    for (const auto& [idx, node] : Enumerate(list->children())) {
      StringIR* str = static_cast<StringIR*>(node);
      PL_ASSIGN_OR_RETURN(ColumnIR * col, graph->MakeNode<ColumnIR>());
      PL_RETURN_IF_ERROR(col->Init(str->str(), parent_index, str->ast_node()));
      columns[idx] = col;
    }
    return columns;
  } else if (!Match(node, String())) {
    return node->CreateIRNodeError("'$0' must be a label or a list of labels", arg_name);
  }
  StringIR* str = static_cast<StringIR*>(node);
  PL_ASSIGN_OR_RETURN(ColumnIR * col, graph->MakeNode<ColumnIR>());
  PL_RETURN_IF_ERROR(col->Init(str->str(), parent_index, str->ast_node()));
  return std::vector<ColumnIR*>{col};
}

StatusOr<QLObjectPtr> AggHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                       const ParsedArgs& args) {
  // converts the mapping of args.kwargs into ColExpressionvector
  ColExpressionVector aggregate_expressions;
  for (const auto& [name, expr] : args.kwargs()) {
    if (!Match(expr, Tuple())) {
      return expr->CreateIRNodeError("Expected '$0' kwarg argument to be a tuple, not $1",
                                     Dataframe::kBlockingAggOpId, expr->type_string());
    }
    PL_ASSIGN_OR_RETURN(FuncIR * parsed_expr,
                        ParseNameTuple(df->graph(), static_cast<TupleIR*>(expr)));
    aggregate_expressions.push_back({name, parsed_expr});
  }

  PL_ASSIGN_OR_RETURN(BlockingAggIR * agg_op, df->graph()->MakeNode<BlockingAggIR>(ast));
  PL_RETURN_IF_ERROR(agg_op->Init(df->op(), {}, aggregate_expressions));
  return StatusOr(std::make_shared<Dataframe>(agg_op));
}

StatusOr<FuncIR*> AggHandler::ParseNameTuple(IR* ir, TupleIR* tuple) {
  DCHECK_EQ(tuple->children().size(), 2UL);
  IRNode* childone = tuple->children()[0];
  IRNode* childtwo = tuple->children()[1];
  if (!Match(childone, String())) {
    return childone->CreateIRNodeError("Expected 'str' for first tuple argument. Received '$0'",
                                       childone->type_string());
  }

  if (!Match(childtwo, Func())) {
    return childtwo->CreateIRNodeError("Expected 'func' for second tuple argument. Received '$0'",
                                       childtwo->type_string());
  }

  std::string argcol_name = static_cast<StringIR*>(childone)->str();
  FuncIR* func = static_cast<FuncIR*>(childtwo);
  // The function should be specified as a single function by itself.
  // This could change in the future.
  if (func->args().size() != 0) {
    return func->CreateIRNodeError("Expected function to not have specified arguments");
  }
  PL_ASSIGN_OR_RETURN(ColumnIR * argcol, ir->MakeNode<ColumnIR>(childone->ast_node()));
  // TODO(philkuz) remove ast_node init arguemnt upon refactoring ast node placement.
  // parent_op_idx is 0 because we only have one parent for an aggregate.
  PL_RETURN_IF_ERROR(argcol->Init(argcol_name, /* parent_op_idx */ 0, childone->ast_node()));
  PL_RETURN_IF_ERROR(func->AddArg(argcol));
  return func;
}

StatusOr<QLObjectPtr> RangeHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                         const ParsedArgs& args) {
  IRNode* start_repr = args.GetArg("start");
  IRNode* stop_repr = args.GetArg("stop");
  if (!Match(start_repr, Expression())) {
    return start_repr->CreateIRNodeError("'start' must be an expression");
  }

  if (!Match(stop_repr, Expression())) {
    return stop_repr->CreateIRNodeError("'stop' must be an expression");
  }

  ExpressionIR* start_expr = static_cast<ExpressionIR*>(start_repr);
  ExpressionIR* stop_expr = static_cast<ExpressionIR*>(stop_repr);

  PL_ASSIGN_OR_RETURN(RangeIR * range_op, df->graph()->MakeNode<RangeIR>(ast));
  PL_RETURN_IF_ERROR(range_op->Init(df->op(), start_expr, stop_expr));
  return StatusOr(std::make_shared<Dataframe>(range_op));
}

StatusOr<QLObjectPtr> OldMapHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                          const ParsedArgs& args) {
  IRNode* lambda_func = args.GetArg("fn");
  if (!Match(lambda_func, Lambda())) {
    return lambda_func->CreateIRNodeError("'fn' must be a lambda");
  }
  LambdaIR* lambda = static_cast<LambdaIR*>(lambda_func);
  if (!lambda->HasDictBody()) {
    return lambda->CreateIRNodeError("'fn' argument error, lambda must have a dictionary body");
  }

  PL_ASSIGN_OR_RETURN(MapIR * map_op, df->graph()->MakeNode<MapIR>(ast));
  PL_RETURN_IF_ERROR(map_op->Init(df->op(), lambda->col_exprs()));
  // Delete the lambda.
  PL_RETURN_IF_ERROR(df->graph()->DeleteNode(lambda->id()));
  return StatusOr(std::make_shared<Dataframe>(map_op));
}

StatusOr<QLObjectPtr> OldFilterHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                             const ParsedArgs& args) {
  IRNode* lambda_func = args.GetArg("fn");
  if (!Match(lambda_func, Lambda())) {
    return lambda_func->CreateIRNodeError("'fn' must be a lambda");
  }

  LambdaIR* lambda = static_cast<LambdaIR*>(lambda_func);
  if (lambda->HasDictBody()) {
    return lambda->CreateIRNodeError("'fn' argument error, lambda cannot have a dictionary body");
  }

  // Have to remove the edges from the Lambda
  PL_ASSIGN_OR_RETURN(ExpressionIR * expr, lambda->GetDefaultExpr());

  PL_ASSIGN_OR_RETURN(FilterIR * filter_op, df->graph()->MakeNode<FilterIR>(ast));
  PL_RETURN_IF_ERROR(filter_op->Init(df->op(), expr));
  // Delete the lambda.
  PL_RETURN_IF_ERROR(df->graph()->DeleteNode(lambda->id()));
  return StatusOr(std::make_shared<Dataframe>(filter_op));
}

StatusOr<QLObjectPtr> LimitHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                         const ParsedArgs& args) {
  // TODO(philkuz) (PL-1161) Add support for compile time evaluation of Limit argument.
  IRNode* rows_node = args.GetArg("rows");
  if (!Match(rows_node, Int())) {
    return rows_node->CreateIRNodeError("'rows' must be an int");
  }
  int64_t limit_value = static_cast<IntIR*>(rows_node)->val();

  PL_ASSIGN_OR_RETURN(LimitIR * limit_op, df->graph()->MakeNode<LimitIR>(ast));
  PL_RETURN_IF_ERROR(limit_op->Init(df->op(), limit_value));
  // Delete the integer node.
  PL_RETURN_IF_ERROR(df->graph()->DeleteNode(rows_node->id()));
  return StatusOr(std::make_shared<Dataframe>(limit_op));
}

StatusOr<QLObjectPtr> OldAggHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                          const ParsedArgs& args) {
  IRNode* by_func = args.GetArg("by");
  IRNode* fn_func = args.GetArg("fn");
  if (!Match(by_func, Lambda())) {
    return by_func->CreateIRNodeError("'by' must be a lambda");
  }
  if (!Match(fn_func, Lambda())) {
    return fn_func->CreateIRNodeError("'fn' must be a lambda");
  }
  LambdaIR* fn = static_cast<LambdaIR*>(fn_func);
  if (!fn->HasDictBody()) {
    return fn->CreateIRNodeError("'fn' argument error, lambda must have a dictionary body");
  }

  LambdaIR* by = static_cast<LambdaIR*>(by_func);
  if (by->HasDictBody()) {
    return by->CreateIRNodeError("'by' argument error, lambda cannot have a dictionary body");
  }

  // Have to remove the edges from the by lambda.
  PL_ASSIGN_OR_RETURN(ExpressionIR * by_expr, by->GetDefaultExpr());
  PL_ASSIGN_OR_RETURN(std::vector<ColumnIR*> groups, SetupGroups(by_expr));

  PL_ASSIGN_OR_RETURN(BlockingAggIR * agg_op, df->graph()->MakeNode<BlockingAggIR>(ast));
  PL_RETURN_IF_ERROR(agg_op->Init(df->op(), groups, fn->col_exprs()));
  // Delete the by.
  PL_RETURN_IF_ERROR(df->graph()->DeleteNode(by->id()));
  PL_RETURN_IF_ERROR(df->graph()->DeleteNode(fn->id()));
  return StatusOr(std::make_shared<Dataframe>(agg_op));
}

StatusOr<std::vector<ColumnIR*>> OldAggHandler::SetupGroups(ExpressionIR* group_by_expr) {
  std::vector<ColumnIR*> groups;
  if (Match(group_by_expr, ListWithChildren(ColumnNode()))) {
    for (ExpressionIR* child : static_cast<ListIR*>(group_by_expr)->children()) {
      groups.push_back(static_cast<ColumnIR*>(child));
    }
    PL_RETURN_IF_ERROR(group_by_expr->graph_ptr()->DeleteNode(group_by_expr->id()));
  } else if (Match(group_by_expr, ColumnNode())) {
    groups.push_back(static_cast<ColumnIR*>(group_by_expr));
  } else {
    return group_by_expr->CreateIRNodeError(
        "'by' lambda must contain a column or a list of columns");
  }
  return groups;
}

StatusOr<QLObjectPtr> OldJoinHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                           const ParsedArgs& args) {
  IRNode* right_node = args.GetArg("right");
  IRNode* type_node = args.GetArg("type");
  IRNode* cond_node = args.GetArg("cond");
  IRNode* cols_node = args.GetArg("cols");

  if (!Match(right_node, Operator())) {
    return right_node->CreateIRNodeError("'right' must be a Dataframe");
  }
  if (!Match(cond_node, Lambda())) {
    return cond_node->CreateIRNodeError("'cond' must be a lambda");
  }
  if (!Match(cols_node, Lambda())) {
    return cols_node->CreateIRNodeError("'cols' must be a lambda");
  }
  if (!Match(type_node, String())) {
    return type_node->CreateIRNodeError("'type' must be a str");
  }

  OperatorIR* right = static_cast<OperatorIR*>(right_node);

  LambdaIR* cols = static_cast<LambdaIR*>(cols_node);
  if (!cols->HasDictBody()) {
    return cols->CreateIRNodeError("'cols' argument error, lambda must have a dictionary body");
  }

  LambdaIR* cond = static_cast<LambdaIR*>(cond_node);
  if (cond->HasDictBody()) {
    return cond->CreateIRNodeError("'cond' argument error, lambda cannot have a dictionary body");
  }

  std::string how_str = static_cast<StringIR*>(type_node)->str();
  PL_RETURN_IF_ERROR(df->graph()->DeleteNode(type_node->id()));

  std::vector<ColumnIR*> columns;
  std::vector<std::string> column_names;
  // Have to remove the edges from the fn lambda.
  for (const ColumnExpression& mapped_expression : cols->col_exprs()) {
    ExpressionIR* expr = mapped_expression.node;
    if (!Match(expr, ColumnNode())) {
      return expr->CreateIRNodeError("'cols' can only have columns");
    }
    column_names.push_back(mapped_expression.name);
    columns.push_back(static_cast<ColumnIR*>(expr));
  }

  // Have to remove the edges from the by lambda.
  PL_ASSIGN_OR_RETURN(ExpressionIR * cond_expr, cond->GetDefaultExpr());
  PL_ASSIGN_OR_RETURN(JoinIR::EqConditionColumns eq_condition, JoinIR::ParseCondition(cond_expr));

  PL_ASSIGN_OR_RETURN(JoinIR * join_op, df->graph()->MakeNode<JoinIR>(ast));
  PL_RETURN_IF_ERROR(join_op->Init({df->op(), right}, how_str, eq_condition.left_on_cols,
                                   eq_condition.right_on_cols, {}));
  PL_RETURN_IF_ERROR(join_op->SetOutputColumns(column_names, columns));
  // Delete the lambdas.
  PL_RETURN_IF_ERROR(df->graph()->DeleteNode(cond_node->id()));
  PL_RETURN_IF_ERROR(df->graph()->DeleteNode(cols_node->id()));
  return StatusOr(std::make_shared<Dataframe>(join_op));
}

StatusOr<QLObjectPtr> OldResultHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
                                             const ParsedArgs& args) {
  IRNode* name_node = args.GetArg("name");
  if (!Match(name_node, String())) {
    return name_node->CreateIRNodeError("'name' must be a str");
  }
  std::string name = static_cast<StringIR*>(name_node)->str();
  PL_ASSIGN_OR_RETURN(MemorySinkIR * sink_op, df->graph()->MakeNode<MemorySinkIR>(ast));
  PL_RETURN_IF_ERROR(sink_op->Init(df->op(), name, {}));
  return StatusOr(std::make_shared<NoneObject>(sink_op));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
