// Camera.ixx - Isometric camera for the 3D world (pure math, no graphics API)
export module LB.RenderingSystem:Camera;

import std;

export namespace LB::Systems::Rendering {

    /// <summary>
    /// A point or direction in 3D world space. Right-handed, Y up: the ground plane is XZ and a
    /// ground point (x, y) in world tiles is (x, 0, y), so +Z points "south" (screen-down at yaw 0).
    /// </summary>
    struct Vector3 {
        float x{0.0f};
        float y{0.0f};
        float z{0.0f};
    };

    /// <summary>
    /// A point on the ground plane, in world tiles
    /// </summary>
    struct GroundPoint {
        float x{0.0f};
        float y{0.0f};
    };

    /// <summary>
    /// Where a world point lands on screen, in pixels from the top-left corner
    /// </summary>
    struct ScreenPoint {
        float x{0.0f};
        float y{0.0f};
        bool inFront{false};   // false when the point is behind the camera (x, y meaningless)
    };

    /// <summary>
    /// Everything the renderer needs to build the view and projection matrices
    /// (right-handed look-at, perspective with a vertical field of view)
    /// </summary>
    struct CameraView {
        Vector3 eye{0.0f, 10.0f, 10.0f};
        Vector3 target{};
        float verticalFovRadians{0.5235988f};
        float nearPlane{0.5f};
        float farPlane{200.0f};
    };

    /// <summary>
    /// Zoom levels from game-design.md (Perspective & Camera)
    /// </summary>
    enum class ZoomLevel {
        Close = 0,  // character-focused, tactical detail
        Medium,     // default
        Wide        // situational awareness
    };

    /// <summary>
    /// Isometric follow camera (game-design.md, Perspective & Camera):
    /// - fixed downward pitch, perspective projection with a narrow field of view;
    /// - 90 degree rotation steps with a short ease;
    /// - three zoom levels, eased;
    /// - soft follow with a dead zone: the player moves freely inside a circle around the focus
    ///   and pushes the camera when reaching its edge; SnapTo refocuses instantly.
    /// Yaw is the camera's bearing around the focus seen from above: at 0 the camera is south of
    /// the focus looking north, so screen-right is +X and screen-up is -Z.
    /// </summary>
    class IsometricCamera {
    public:
        static constexpr float PitchDegrees = 40.0f;
        static constexpr float DefaultYawDegrees = 45.0f;
        static constexpr float FieldOfViewDegrees = 30.0f;
        static constexpr float DeadZoneRadius = 1.5f;       // world tiles
        static constexpr float RotationSeconds = 0.25f;
        static constexpr float ZoomRate = 10.0f;            // exponential ease, per second
        static constexpr float CloseDistance = 14.0f;
        static constexpr float MediumDistance = 20.0f;
        static constexpr float WideDistance = 28.0f;

        /// <summary>
        /// Camera distance from the focus for a zoom level
        /// </summary>
        static constexpr float DistanceFor(ZoomLevel zoom) noexcept {
            switch (zoom) {
                case ZoomLevel::Close: return CloseDistance;
                case ZoomLevel::Medium: return MediumDistance;
                case ZoomLevel::Wide: return WideDistance;
            }
            return MediumDistance;
        }

        /// <summary>
        /// Move the focus to a ground point immediately (spawn, teleport, load)
        /// </summary>
        void SnapTo(GroundPoint point) noexcept {
            focus_ = point;
        }

        /// <summary>
        /// Keep a ground point (the player) inside the dead zone, pushing the focus when needed
        /// </summary>
        void Follow(GroundPoint point) noexcept {
            const float dx = point.x - focus_.x;
            const float dy = point.y - focus_.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= DeadZoneRadius) return;

            const float push = (distance - DeadZoneRadius) / distance;
            focus_.x += dx * push;
            focus_.y += dy * push;
        }

        /// <summary>
        /// Rotate the view 90 degrees; controls switch to the new orientation immediately while
        /// the view eases over RotationSeconds
        /// </summary>
        void RotateClockwise() noexcept { StartRotation(90.0f); }
        void RotateCounterClockwise() noexcept { StartRotation(-90.0f); }

        void SetZoom(ZoomLevel zoom) noexcept { zoom_ = zoom; }

        void ZoomIn() noexcept {
            if (zoom_ != ZoomLevel::Close) zoom_ = static_cast<ZoomLevel>(static_cast<int>(zoom_) - 1);
        }

        void ZoomOut() noexcept {
            if (zoom_ != ZoomLevel::Wide) zoom_ = static_cast<ZoomLevel>(static_cast<int>(zoom_) + 1);
        }

        /// <summary>
        /// Advance the rotation and zoom eases
        /// </summary>
        void Update(float elapsedSeconds) noexcept {
            if (rotationElapsed_ < RotationSeconds) {
                rotationElapsed_ = std::min(RotationSeconds, rotationElapsed_ + elapsedSeconds);
            }

            const float targetDistance = DistanceFor(zoom_);
            const float blend = std::min(1.0f, elapsedSeconds * ZoomRate);
            distance_ += (targetDistance - distance_) * blend;
            if (std::abs(targetDistance - distance_) < 0.01f) distance_ = targetDistance;
        }

        /// <summary>
        /// Yaw currently shown (mid-ease while rotating), in degrees
        /// </summary>
        [[nodiscard]] float GetYawDegrees() const noexcept {
            const float t = rotationElapsed_ / RotationSeconds;
            const float eased = t * t * (3.0f - 2.0f * t);
            return rotationFrom_ + (yaw_ - rotationFrom_) * eased;
        }

        /// <summary>
        /// Yaw the camera is rotating to (what the controls use), in degrees
        /// </summary>
        [[nodiscard]] float GetTargetYawDegrees() const noexcept { return yaw_; }

        [[nodiscard]] bool IsRotating() const noexcept { return rotationElapsed_ < RotationSeconds; }
        [[nodiscard]] ZoomLevel GetZoom() const noexcept { return zoom_; }
        [[nodiscard]] float GetDistance() const noexcept { return distance_; }
        [[nodiscard]] GroundPoint GetFocus() const noexcept { return focus_; }

        /// <summary>
        /// Turn a screen-relative direction (right, up) into a ground direction for the target
        /// yaw: holding "up" walks away from the camera whatever the rotation
        /// </summary>
        [[nodiscard]] GroundPoint ScreenToGround(float right, float up) const noexcept {
            const float yaw = Radians(yaw_);
            const float s = std::sin(yaw);
            const float c = std::cos(yaw);
            return GroundPoint{ right * c - up * s, -right * s - up * c };
        }

        /// <summary>
        /// Eye, target, and projection parameters for the current (eased) state
        /// </summary>
        [[nodiscard]] CameraView GetView() const noexcept {
            const float yaw = Radians(GetYawDegrees());
            const float pitch = Radians(PitchDegrees);

            CameraView view{};
            view.target = Vector3{ focus_.x, 0.0f, focus_.y };
            view.eye = Vector3{
                focus_.x + distance_ * std::cos(pitch) * std::sin(yaw),
                distance_ * std::sin(pitch),
                focus_.y + distance_ * std::cos(pitch) * std::cos(yaw) };
            view.verticalFovRadians = Radians(FieldOfViewDegrees);
            return view;
        }

        /// <summary>
        /// Project a world point to the screen (same math as the GPU's view and projection)
        /// </summary>
        [[nodiscard]] static ScreenPoint WorldToScreen(const CameraView& view, Vector3 point, float screenWidth, float screenHeight) noexcept {
            const Vector3 forward = Normalize(Subtract(view.target, view.eye));
            const Vector3 right = Normalize(Cross(forward, Vector3{ 0.0f, 1.0f, 0.0f }));
            const Vector3 up = Cross(right, forward);

            const Vector3 offset = Subtract(point, view.eye);
            const float depth = Dot(offset, forward);
            if (depth <= view.nearPlane || screenWidth <= 0.0f || screenHeight <= 0.0f) {
                return ScreenPoint{};
            }

            const float tanHalfFov = std::tan(view.verticalFovRadians * 0.5f);
            const float aspect = screenWidth / screenHeight;
            const float ndcX = Dot(offset, right) / (depth * tanHalfFov * aspect);
            const float ndcY = Dot(offset, up) / (depth * tanHalfFov);
            return ScreenPoint{ (ndcX + 1.0f) * 0.5f * screenWidth, (1.0f - ndcY) * 0.5f * screenHeight, true };
        }

    private:
        GroundPoint focus_{};
        float yaw_{DefaultYawDegrees};
        float rotationFrom_{DefaultYawDegrees};
        float rotationElapsed_{RotationSeconds};
        ZoomLevel zoom_{ZoomLevel::Medium};
        float distance_{MediumDistance};

        void StartRotation(float degrees) noexcept {
            // A rotation started mid-ease continues from the yaw currently shown
            rotationFrom_ = GetYawDegrees();
            yaw_ += degrees;
            rotationElapsed_ = 0.0f;

            // Keep angles bounded; shift both ends so the ease is unchanged
            if (yaw_ >= 360.0f || yaw_ < 0.0f) {
                const float wrap = yaw_ >= 360.0f ? -360.0f : 360.0f;
                yaw_ += wrap;
                rotationFrom_ += wrap;
            }
        }

        static constexpr float Radians(float degrees) noexcept {
            return degrees * (std::numbers::pi_v<float> / 180.0f);
        }

        static constexpr Vector3 Subtract(Vector3 a, Vector3 b) noexcept { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
        static constexpr float Dot(Vector3 a, Vector3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
        static constexpr Vector3 Cross(Vector3 a, Vector3 b) noexcept {
            return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
        }
        static Vector3 Normalize(Vector3 v) noexcept {
            const float length = std::sqrt(Dot(v, v));
            return length > 0.0f ? Vector3{ v.x / length, v.y / length, v.z / length } : v;
        }
    };

} // namespace LB::Systems::Rendering
