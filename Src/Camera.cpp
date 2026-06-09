#include "Camera.h"
#include "Game.h"

// TODO: Move this to the camera class and use the rotation vector form GameObject as we will want to allow for rotation along y aswell.
Mat4 Camera::GetViewMatrix() const {
        // return glm::translate(Mat4(1.0f), game_object.position);

        Mat4 view = {
                game_object.right_direction.x, game_object.right_direction.y, game_object.right_direction.z, -(glm::dot(game_object.right_direction, game_object.position)),
                game_object.up_direction.x, game_object.up_direction.y, game_object.up_direction.z, -(glm::dot(game_object.up_direction, game_object.position)),
                game_object.forward_direction.x, game_object.forward_direction.y, game_object.forward_direction.z, -(glm::dot(game_object.forward_direction, game_object.position)),
                0, 0, 0, 1.0f,
        };

        return glm::transpose(view);
}

Mat4 Camera::GetProjectionMatrix() const {
        Mat4 projection {};

        // projection[0][0] = 2 * near;

        // projection[1][1] = 2 * near;

        // projection[2][0] = 0;
        // projection[2][1] = 0;
        // projection[2][2] = (far + near) / (far - near);
        // projection[2][3] = 1;

        // projection[3][2] = -(2 * far * near) / (far - near);
        // projection[3][3] = 0;

        float c = 1.0f / (tanf((fov * (PI_F / 180)) / 2.0f));

        projection[0][0] = c / aspect_ratio;

        projection[1][1] = c;

        projection[2][2] = (far + near) / (far - near);
        projection[2][3] = 1;

        projection[3][2] = -(2 * far * near) / (far - near);
        projection[3][3] = 0;

        return projection;
}

// Need to pass the owner here.
void DefaultController::Init(GameContext& _game_context, GameObject& _owner) {
        game_context = &_game_context;

        // Check if Input Actions already exist
        forward_action = game_context->input_system.GetAction("Forward");
        backward_action = game_context->input_system.GetAction("Backward");
        left_action = game_context->input_system.GetAction("Left");
        right_action = game_context->input_system.GetAction("Right");
        up_action = game_context->input_system.GetAction("Up");
        down_action = game_context->input_system.GetAction("Down");

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
                up_action->AddKey(KeyCode::Q);
        }
        if (!down_action) {
                down_action = game_context->input_system.RegisterAction("Down", ActionType::Scalar);
                down_action->AddKey(KeyCode::E);
        }

        owner = &_owner;

        owner->forward_direction = {0, 0, 1.0f};
        owner->right_direction = {1.0f, 0, 0.0f};
        owner->up_direction = {0.0f, 1.0f, 0.0f};
}

void DefaultController::Shutdown() {

}

// Do we just directly modify the position of the object?
void DefaultController::Update(float delta_time) {
        Vec3 movement{};

        movement += forward_action->GetScalar() * owner->forward_direction;
        movement -= backward_action->GetScalar() * owner->forward_direction;
        movement += right_action->GetScalar() * owner->right_direction;
        movement -= left_action->GetScalar() * owner->right_direction;
        movement += up_action->GetScalar() * owner->up_direction;
        movement -= down_action->GetScalar() * owner->up_direction;


        if (movement.x != 0 and movement.y != 0) movement = glm::normalize(movement);

        movement *= movement_speed * delta_time;

        // Position += movement

        owner->position += movement;

        // Rotate with the mouse delta
        // TODO: Fix choppy mouse movement. Seems slow movements arnt registered.
        Vec2 mouse_movement = game_context->input_system.mouse.delta * delta_time * 100.0f;

        owner->rotation.x += mouse_movement.x;
        while (owner->rotation.x < 0) {
                owner->rotation.x += 2.0f * PI_F;
        }
        while (owner->rotation.x >= 2.0f * PI_F) {
                owner->rotation.x -= 2.0f * PI_F;
        }


        owner->rotation.y += mouse_movement.y;
        owner->rotation.y = std::clamp(owner->rotation.y, -1.5f, 1.5f);

        // std::println("Mouse: {}, {}", mouse_movement.x, mouse_movement.y);

        // Probably want to update forward, right, and up directions.

        // Currenly just keep up as +y, will probably want to orietate it around the planet
        owner->up_direction = Vec3{0, 1.0f, 0.0f};
        // Right wants to be +x rotated by (yaw in +z)
        // NOTE: Only works whilst were upright, after that will need to do extra work.
        // Can just rotate +x around the difference between up and +z?
        // Is it not just +x rotated by pitch around +z? no, if up is +x and we dont move camera its wrong.
        // Use frames and just convert +x from +z space to up space. and then rotate around up by pitch.
        Vec3 x_vec = Vec3{1.0f, 0, 0};
        owner->right_direction = glm::rotate(x_vec, owner->rotation.x, owner->up_direction);
        // Forward is just Cross of up and right. Up X Right
        owner->forward_direction = -glm::cross(owner->up_direction, owner->right_direction);
        owner->forward_direction = glm::rotate(owner->forward_direction, owner->rotation.y, owner->right_direction);

        owner->up_direction = glm::cross(owner->right_direction, owner->forward_direction);

        // std::println("Movement: {}, {}", movement.x, movement.y);
}