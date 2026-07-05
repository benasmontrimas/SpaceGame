#include "Camera.h"
#include "Game.h"

void Camera::Init(GameContext* _game_context) {
        game_context = _game_context;
}

void Camera::Update() {
        aspect_ratio = (float)game_context->window.width / (float)game_context->window.height;
}

// TODO: Move this to the camera class and use the rotation vector form GameObject as we will want to allow for rotation along y aswell.
Mat4 Camera::GetViewMatrix() const {

        Vec3 up_direction      = rotate(game_object.rotation, Vec3{ 0.0f, 1.0f, 0.0f });
        Vec3 right_direction   = rotate(game_object.rotation, Vec3{ 1.0f, 0.0f, 0.0f });
        Vec3 forward_direction = rotate(game_object.rotation, Vec3{ 0.0f, 0.0f, 1.0f });

        Mat4 view = {
                right_direction.x,
                right_direction.y,
                right_direction.z,
                -(glm::dot(right_direction, game_object.position)),
                up_direction.x,
                up_direction.y,
                up_direction.z,
                -(glm::dot(up_direction, game_object.position)),
                forward_direction.x,
                forward_direction.y,
                forward_direction.z,
                -(glm::dot(forward_direction, game_object.position)),
                0,
                0,
                0,
                1.0f,
        };

        return glm::transpose(view);
}

Mat4 Camera::GetProjectionMatrix() const {
        Mat4 projection{};

        float c = 1.0f / (tanf((fov * (PI_F / 180)) / 2.0f));

        projection[0][0] = c / aspect_ratio;

        projection[1][1] = -c;

        projection[2][2] = (far) / (far - near);
        projection[2][3] = 1;

        projection[3][2] = -(far * near) / (far - near);
        projection[3][3] = 0;

        return projection;
}

// Need to pass the owner here.
void DefaultController::Init(GameContext& _game_context, Transform& _owner) {
        game_context = &_game_context;

        // Check if Input Actions already exist
        forward_action  = game_context->input_system.GetAction("Forward");
        backward_action = game_context->input_system.GetAction("Backward");
        left_action     = game_context->input_system.GetAction("Left");
        right_action    = game_context->input_system.GetAction("Right");
        up_action       = game_context->input_system.GetAction("Up");
        down_action     = game_context->input_system.GetAction("Down");

        // If not add them
        if (!forward_action) {
                forward_action = game_context->input_system.RegisterAction("Forward", ActionType::Scalar);
                forward_action->AddKey(KeyCode::W);
        }
        if (!backward_action) {
                backward_action = game_context->input_system.RegisterAction("Backward", ActionType::Scalar);
                backward_action->AddKey(KeyCode::S);
        }
        if (!left_action) {
                left_action = game_context->input_system.RegisterAction("Left", ActionType::Scalar);
                left_action->AddKey(KeyCode::A);
        }
        if (!right_action) {
                right_action = game_context->input_system.RegisterAction("Right", ActionType::Scalar);
                right_action->AddKey(KeyCode::D);
        }
        if (!up_action) {
                up_action = game_context->input_system.RegisterAction("Up", ActionType::Scalar);
                up_action->AddKey(KeyCode::E);
        }
        if (!down_action) {
                down_action = game_context->input_system.RegisterAction("Down", ActionType::Scalar);
                down_action->AddKey(KeyCode::Q);
        }

        owner = &_owner;

        owner->rotation = { { 0.0f, 1.0f, 0.0f }, normalize(owner->position) };
}

void DefaultController::Shutdown() {
}

// Do we just directly modify the position of the object?
void DefaultController::Update(float delta_time) {

        // ===== Update Rotation ===== //

        // == Get World Up == //

        Vec3 new_world_up = glm::normalize(owner->position); // Assuming planet at 0,0,0

        // == Rotation in world == //

        Quat world_rot{ world_up, new_world_up };
        world_up        = new_world_up;
        owner->rotation = world_rot * owner->rotation;
        owner->rotation = glm::normalize(owner->rotation);

        // == Camera Rotation == //

        Vec3 right = rotate(owner->rotation, Vec3{ 1.0f, 0.0f, 0.0f });

        Vec2 mouse_delta = game_context->input_system.mouse.delta * mouse_sensitivity;

        Quat yaw_rot   = glm::angleAxis(mouse_delta.x, world_up);
        Quat pitch_rot = glm::angleAxis(mouse_delta.y, right);

        owner->rotation = glm::normalize(pitch_rot * yaw_rot * owner->rotation);

        // ===== Movement ===== //

        Vec3 movement_direction = {
                right_action->GetScalar() - left_action->GetScalar(),
                up_action->GetScalar() - down_action->GetScalar(),
                forward_action->GetScalar() - backward_action->GetScalar(),
        };

        if (length2(movement_direction) > 0.0f) {
                movement_direction = normalize(movement_direction);
                Vec3 movement      = movement_direction * movement_speed * delta_time;
                owner->position += rotate(owner->rotation, movement);
        }

        // ===== Change Movement Speed with Mouse Wheel ===== //

        Vec2 mouse_scroll = game_context->input_system.mouse.wheel_delta;
        movement_speed += mouse_scroll.y * 100;
        movement_speed = std::clamp(movement_speed, 10.0f, 10000.0f);
}