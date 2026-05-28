#include <print>
#include <filesystem>
#include <vector>

#include "slang/slang-com-ptr.h"
#include "slang/slang.h"

#include "Base.h"

/*
TODO:
- Add User Attributes
- Add Modifiers
*/


void PrintVariable(slang::VariableReflection* variable, const std::string& prefix);
void PrintLayout(slang::VariableLayoutReflection layout, const std::string& prefix);
void PrintArray(slang::TypeReflection* type, const std::string& prefix);
void PrintStruct(slang::TypeReflection* type, const std::string& prefix);
void PrintScalar(slang::TypeReflection* type, const std::string& prefix);
void PrintType(slang::TypeReflection* type, const std::string& prefix);



constexpr const char* spacing = "\t";

enum class ReturnCode : int {
        Ok,
        NoInput,
        CantFind,
        CompileError,
};

int main(int argc, const char* argv[]) {
        if (argc < 2) {
                std::println("Requires a shader file as input");
                return (int)ReturnCode::NoInput;
        }

        std::filesystem::path shader_folder = "Assets/Shaders/";
        std::filesystem::path shader_file = argv[1];

        std::println("{}", (shader_folder / shader_file).c_str());

        if (!std::filesystem::exists(shader_file)) {
                if (!std::filesystem::exists(shader_folder / shader_file)) {
                        std::println("Cannot find shader file");
                        return (int)ReturnCode::CantFind;
                }

                shader_file = shader_folder / shader_file;
        }

        // ===== Initialise Slang ===== //
        Slang::ComPtr<slang::IGlobalSession> slang_global_session;
        Slang::ComPtr<slang::ISession>       slang_session;

        slang::createGlobalSession(slang_global_session.writeRef());

        // ===== Create Slang Session ===== //

        auto slang_targets{
                std::to_array<slang::TargetDesc>({
                        {
                                .format  = SLANG_SPIRV,
                                .profile = slang_global_session->findProfile("spirv_1_4"),
                        },
                })
        };

        auto slang_options{ std::to_array<slang::CompilerOptionEntry>({ {
                slang::CompilerOptionName::EmitSpirvDirectly,
                { slang::CompilerOptionValueKind::Int, 1 },
        } }) };

        const char* shader_search_paths[] = { "Assets/Shaders/" };

        slang::SessionDesc slang_session_desc{
                .targets                  = slang_targets.data(),
                .targetCount              = SlangInt(slang_targets.size()),
                .defaultMatrixLayoutMode  = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                .searchPaths              = shader_search_paths,
                .searchPathCount          = 1,
                .compilerOptionEntries    = slang_options.data(),
                .compilerOptionEntryCount = u32(slang_options.size()),
        };

        slang_global_session->createSession(slang_session_desc, slang_session.writeRef());

        // ===== Load Shader ===== //
        Slang::ComPtr<slang::IBlob>   diagnostics;
        Slang::ComPtr<slang::IModule> module{ slang_session->loadModuleFromSource("base", shader_file.c_str(), nullptr,
                                                                                        diagnostics.writeRef()) };
        if (diagnostics) {
                std::println("Shader Compile Error: {}", (const char*)diagnostics->getBufferPointer());
                return (int)ReturnCode::CompileError;
        }

        // ===== Link Then Get Program Layout ===== //

        u32 entry_point_count = module->getDefinedEntryPointCount();

        std::vector<Slang::ComPtr<slang::IComponentType>> components;

        for (u32 i = 0; i < entry_point_count; i++) {
                Slang::ComPtr<slang::IEntryPoint> entry_point;
                module->getDefinedEntryPoint(i, entry_point.writeRef());
                std::println("Entry Point: {}", entry_point->getFunctionReflection()->getName());
                components.push_back(Slang::ComPtr<slang::IComponentType>(entry_point.get()));
        }

        Slang::ComPtr<slang::IComponentType> composed;
        slang_session->createCompositeComponentType(
                (slang::IComponentType**)components.data(),
                components.size(),
                composed.writeRef(),
                diagnostics.writeRef()
        );

        if (diagnostics) {
                std::println("Shader Composite Failed: {}", (const char*)diagnostics->getBufferPointer());
                return (int)ReturnCode::CompileError;
        }

        Slang::ComPtr<slang::IComponentType> program;
        composed->link(program.writeRef(), diagnostics.writeRef());

        if (diagnostics) {
                std::println("Shader Link Failed: {}", (const char*)diagnostics->getBufferPointer());
                return (int)ReturnCode::CompileError;
        }

        slang::ProgramLayout* program_layout = program->getLayout();

        u32 entry_count = program_layout->getEntryPointCount();

        for (u32 i = 0; i < entry_count; i++) {
                slang::EntryPointReflection* entry_point = program_layout->getEntryPointByIndex(i);

                std::println("Name: {}", entry_point->getName());

                u32 variable_count = entry_point->getParameterCount();

                for (u32 v = 0; v < variable_count; v++) {
                        slang::VariableLayoutReflection* variable = entry_point->getParameterByIndex(v);

                        std::println("  Name: {}", variable->getName());
                }
        }

        slang::printProgramLayout(program_layout, SLANG_SPIRV);
}


// ===== Print Functions ===== //


void PrintVariable(slang::VariableReflection* variable, const std::string& prefix) {
        std::print("{} {}: ", prefix, variable->getName());

        slang::TypeReflection* type = variable->getType();

        std::string next_prefix = prefix + spacing;
        PrintType(type, prefix);
}

void PrintLayout(slang::VariableLayoutReflection* layout, const std::string& prefix) {

}

void PrintArray(slang::TypeReflection* type, const std::string& prefix) {
        u32 element_count = type->getElementCount();

        std::string element_count_string;

        if (element_count == SLANG_UNBOUNDED_SIZE) {
                element_count_string = "";
        } else if (element_count == SLANG_UNKNOWN_SIZE) {
                element_count_string = "?";
        } else {
                element_count_string = std::to_string(element_count);
        }

        std::print("{} Array[{}]: ", prefix, element_count_string);

        slang::TypeReflection* element_type = type->getElementType();
        PrintType(element_type, prefix);
}

void PrintStruct(slang::TypeReflection* type, const std::string& prefix) {
        // TODO: Format as struct {name}
        std::print("{} Struct:\n", prefix);

        u32 field_count = type->getFieldCount();

        for (u32 i = 0; i < field_count; i++) {
                slang::VariableReflection* variable = type->getFieldByIndex(i);

                std::string next_prefix = prefix + spacing;

                PrintVariable(variable, next_prefix);

                std::println("");
        }
}

void PrintScalar(slang::TypeReflection* type, const std::string& prefix) {
        auto scalar_type = type->getScalarType();

        switch (scalar_type) {

 #define CASE(TAG)                                              \
                case slang::TypeReflection::TAG:                \
                std::print("{} {}", prefix, #TAG);      \
                break

        CASE(None);
        CASE(Void);
        CASE(Bool);
        CASE(Int32);
        CASE(UInt32);
        CASE(Int64);
        CASE(UInt64);
        CASE(Float16);
        CASE(Float32);
        CASE(Float64);
        CASE(Int8);
        CASE(UInt8);
        CASE(Int16);
        CASE(UInt16);

#undef CASE

        default:
                std::println("{} Unknown Type", prefix);
        }
}

void PrintType(slang::TypeReflection* type, const std::string& prefix) {
        auto variable_kind = type->getKind();
        switch (variable_kind) {
                case slang::TypeReflection::Kind::None:
                        std::println("None");
                        break;
                case slang::TypeReflection::Kind::Struct:
                        PrintStruct(type, prefix);
                        break;
                case slang::TypeReflection::Kind::Array:
                        PrintArray(type, prefix);
                        break;
                case slang::TypeReflection::Kind::Matrix:
                        break;
                case slang::TypeReflection::Kind::Vector:
                        break;
                case slang::TypeReflection::Kind::Scalar:
                        PrintScalar(type, prefix);
                        break;
                case slang::TypeReflection::Kind::ConstantBuffer:
                        break;
                case slang::TypeReflection::Kind::Resource:
                        break;
                case slang::TypeReflection::Kind::SamplerState:
                        break;
                case slang::TypeReflection::Kind::TextureBuffer:
                        break;
                case slang::TypeReflection::Kind::ShaderStorageBuffer:
                        break;
                case slang::TypeReflection::Kind::ParameterBlock:
                        break;
                case slang::TypeReflection::Kind::GenericTypeParameter:
                        break;
                case slang::TypeReflection::Kind::Interface:
                        break;
                case slang::TypeReflection::Kind::OutputStream:
                        break;
                case slang::TypeReflection::Kind::Specialized:
                        break;
                case slang::TypeReflection::Kind::Feedback:
                        break;
                case slang::TypeReflection::Kind::Pointer:
                        break;
                case slang::TypeReflection::Kind::DynamicResource:
                        break;
                default:
                        std::println("Unknown variable kind");
        }
}