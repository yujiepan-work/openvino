// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/op/assign.hpp"

#include "assign_shape_inference.hpp"
#include "itt.hpp"
#include "openvino/op/read_value.hpp"
#include "openvino/op/util/variable.hpp"
#include "openvino/op/util/variable_context.hpp"

namespace ov {
namespace op {
namespace v3 {
Assign::Assign(const Output<Node>& new_value, const std::string& variable_id)
    : AssignBase({new_value}),
      m_variable_id(variable_id) {
    constructor_validate_and_infer_types();
}

void Assign::validate_and_infer_types() {
    OV_OP_SCOPE(v3_Assign_validate_and_infer_types);
    auto value = input_value(0);
    auto arg_t = get_input_element_type(0);
    const auto& input_shape = get_input_partial_shape(0);
    if (!m_variable) {
        NodeVector start_nodes;
        for (const auto& input : inputs()) {
            start_nodes.push_back(input.get_source_output().get_node_shared_ptr());
        }
        auto nodes = topological_sort(start_nodes);
        for (const auto& node : nodes) {
            if (auto read_value = ov::as_type_ptr<v3::ReadValue>(node)) {
                if (read_value->get_variable_id() == m_variable_id)
                    m_variable = read_value->get_variable();
            }
        }
        NODE_VALIDATION_CHECK(this, m_variable != nullptr, "Can't find variable with id = ", m_variable_id);
    }

    const auto input_shapes = std::vector<ov::PartialShape>{input_shape};
    const auto output_shapes = shape_infer(this, input_shapes);
    set_output_type(0, arg_t, output_shapes[0]);
}

std::shared_ptr<Node> Assign::clone_with_new_inputs(const OutputVector& new_args) const {
    OV_OP_SCOPE(v3_Assign_clone_with_new_inputs);
    check_new_args_count(this, new_args);
    return std::make_shared<Assign>(new_args.at(0), m_variable_id);
}

bool Assign::visit_attributes(AttributeVisitor& visitor) {
    OV_OP_SCOPE(v3_Assign_visit_attributes);
    visitor.on_attribute("variable_id", m_variable_id);
    return true;
}
}  // namespace v3

namespace v6 {
Assign::Assign(const Output<Node>& new_value, const std::shared_ptr<util::Variable>& variable)
    : AssignBase({new_value}) {
    m_variable = variable;
    constructor_validate_and_infer_types();
}

void Assign::validate_and_infer_types() {
    OV_OP_SCOPE(v6_Assign_validate_and_infer_types);
    m_variable->update({get_input_partial_shape(0), get_input_element_type(0), m_variable->get_info().variable_id});

    set_output_type(0, get_input_element_type(0), get_input_partial_shape(0));
}

std::shared_ptr<Node> Assign::clone_with_new_inputs(const OutputVector& new_args) const {
    OV_OP_SCOPE(v6_Assign_clone_with_new_inputs);
    check_new_args_count(this, new_args);
    return std::make_shared<Assign>(new_args.at(0), m_variable);
}

bool Assign::visit_attributes(AttributeVisitor& visitor) {
    OV_OP_SCOPE(v6_Assign_visit_attributes);
    visitor.on_attribute("variable_id", m_variable);
    return true;
}

bool Assign::evaluate(TensorVector& outputs,
                      const TensorVector& inputs,
                      const EvaluationContext& evaluation_context) const {
    OV_OP_SCOPE(v6_Assign_evaluate);
    const auto& found_context = evaluation_context.find("VariableContext");
    NODE_VALIDATION_CHECK(this, found_context != evaluation_context.end(), "VariableContext not found.");

    auto& variable_context = const_cast<util::VariableContext&>(found_context->second.as<util::VariableContext>());

    const auto& variable_values = variable_context.get_variable_values();

    // automatically allocate memory if not provided by user
    if (variable_values.find(m_variable) == variable_values.end()) {
        auto tensor = Tensor(m_variable->get_info().data_type, m_variable->get_info().data_shape.to_shape());
        variable_context.set_variable_value(m_variable, std::make_shared<util::VariableValue>(tensor));
    }

    const auto var_value = variable_values.find(m_variable)->second;
    var_value->set_reset(false);
    auto buffer = var_value->get_state();
    buffer.set_shape(inputs[0].get_shape());
    outputs[0].set_shape(inputs[0].get_shape());

    std::memcpy(outputs[0].data(), inputs[0].data(), inputs[0].get_byte_size());
    std::memcpy(buffer.data(), inputs[0].data(), inputs[0].get_byte_size());

    return true;
}

bool Assign::has_evaluate() const {
    OV_OP_SCOPE(v1_Assign_has_evaluate);
    return true;
}

bool Assign::constant_fold(OutputVector& output_values, const OutputVector& inputs_values) {
    return false;
}
}  // namespace v6
}  // namespace op
}  // namespace ov
