#include "Player.h"

#include "GameContext.h"
#include "Planet.h"

void PlayerController::Init(GameContext* _game_context, Planet* _planet, Transform* _owner) {
        game_context = _game_context;
        planet       = _planet;
        owner        = _owner;

        // ===== Actions ===== //

        // == Check if Input Actions already exist == //

        forward_action  = game_context->input_system.GetAction("Forward");
        backward_action = game_context->input_system.GetAction("Backward");
        left_action     = game_context->input_system.GetAction("Left");
        right_action    = game_context->input_system.GetAction("Right");
        jump_action     = game_context->input_system.GetAction("Jump");

        // == If not add them == //

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
        if (!jump_action) {
                jump_action = game_context->input_system.RegisterAction("Jump", ActionType::Scalar);
                jump_action->AddKey(KeyCode::Space);
        }

        world_up = Vec3{ 0.0f, 1.0f, 0.0f };
}

void PlayerController::Update(float delta_time) {
        Vec2 mouse_delta = game_context->input_system.mouse.delta * mouse_sensitivity;
        if (delta_time == 0.0f) mouse_delta *= 0.0f;

        // ===== Movement ===== //

        Vec3 new_world_up = glm::normalize(owner->position);

        Quat frame_world_rotation{ world_up, new_world_up };
        world_rotation = frame_world_rotation * world_rotation;
        world_rotation = glm::normalize(world_rotation);

        world_up = new_world_up;

        // == World Yaw == //

        Quat world_yaw_rot = glm::angleAxis(mouse_delta.x, world_up);
        world_rotation     = world_yaw_rot * world_rotation;

        owner->rotation = frame_world_rotation * owner->rotation;

        // == Camera Rotation == //

        Vec3 right = rotate(world_rotation, Vec3{ 1.0f, 0.0f, 0.0f });


        Quat yaw_rot   = glm::angleAxis(mouse_delta.x, world_up);
        Quat pitch_rot = glm::angleAxis(mouse_delta.y, right);

        owner->rotation = glm::normalize(pitch_rot * yaw_rot * owner->rotation);

        // ===== Check Grounded ===== //

        Ray ray{
                .origin    = owner->position,
                .direction = -world_up,
        };

        float distance = planet->CheckIntersection(ray, 200.0f);
        if (distance != f32_max) {
                if (distance < 20.0f) {
                        is_grounded = true;
                        owner->position += max(((17.0f - distance) * world_up) * delta_time * 10.0f, 0.0f);
                        if (distance < 10.0f) {
                                owner->position += max((17.0f - distance) * world_up * delta_time * 20.0f, 0.0f);
                        }
                        velocity.y = std::max(velocity.y, 0.0f);
                } else {
                        is_grounded = false;
                }
        }

        // ===== Movement ===== //

        Vec3 movement_direction = {
                right_action->GetScalar() - left_action->GetScalar(),
                0.0f,
                forward_action->GetScalar() - backward_action->GetScalar(),
        };

        if (length2(movement_direction) > 0.0f) {
                movement_direction  = normalize(movement_direction);
                float speed_penalty = 1.0f;
                if (!is_grounded) speed_penalty = 0.5f;
                Vec3 movement = movement_direction * movement_speed * delta_time * speed_penalty;
                owner->position += rotate(world_rotation, movement);
        }

        if (is_grounded and jump_action->IsPressed()) {
                velocity.y = 70.0f;

                is_grounded = false;
        }

        if (!is_grounded) {
                velocity.y += -100.0f * delta_time;
                if (velocity.y < -100.0f) velocity.y = -100.0f;
        }

        velocity -= Vec3{ 1.0f, 0.0f, 1.0f } * velocity * 0.1f * delta_time;

        // ===== Velocity ===== //

        owner->position += rotate(world_rotation, velocity) * delta_time;

        if (glm::all(glm::greaterThan(abs(velocity), Vec3{ 0.00001f }))) velocity = Vec3{ 0 };
}

void Player::Init(GameContext* _game_context) {
        game_context       = _game_context;
        transform.position = { 10.0f, 12000.0f, 10.0f };
        camera.Init(game_context);
}

void Player::Start(Planet* planet) {
        transform.position = { 10.0f, 12000.0f, 10.0f };

        Ray ray{
                .origin    = transform.position,
                .direction = -glm::normalize(transform.position),
        };

        float distance = planet->CheckIntersection(ray, 0.0f);
        std::println("Distance: {}", distance);
        while (distance > 3000.0f) {
                planet->Update();
                distance = planet->CheckIntersection(ray, 0.0f);
                std::println("Distance: {}", distance);
        }

        transform.position += ray.direction * (distance - 20);

        controller.Init(game_context, planet, &transform);
}

void Player::Update(float delta_time) {
        controller.Update(delta_time);
        camera.game_object = transform;
        camera.Update();
}