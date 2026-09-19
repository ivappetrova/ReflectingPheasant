#pragma once

#include "GameScene.h"
#include <cstdint>
#include <numbers>

class MyCoolScene final : public GameScene {
public:
	explicit MyCoolScene(const std::string& resourceDir);

	void OnInit(VulkanRenderer* renderer) override;
	void OnUpdate(float deltaTime) override;
	void OnInput(const InputState& input) override;
	void OnGui() override;
	void OnShutdown() override;

private:
	static constexpr float kPi = std::numbers::pi_v<float>;

	////////////////////////////////////////////////////////////////////////////// DATA MEMBERS
	//// Camera
	float m_cameraYaw{ 0.0f };
	float m_cameraPitch{ 0.3f };
	float m_cameraDistance{ 20.0f };
	float m_mouseSensitivity{ 0.005f };

	//// room dimensions
	const float m_HALF_WIDTH_ROOM { 50.f };
	const float m_HALF_LENGHT_ROOM { 70.f };
	const float m_ROOM_HEIGHT{ 30.f };
	
	//// my character
	uint32_t m_Player{ 0 };
	const float m_PLAYER_RADIUS{ 2.f };
	TriVector m_PlayerCenter{ 0.f, m_PLAYER_RADIUS, 0.f };
	const float m_PLAYER_SPEED{ 20.f };

	//// Mini sphere
	uint32_t m_miniSphereId{ 0 };
	const float m_MINI_SPHERE_RADIUS{ 0.5f };
	const float m_MINI_SPHERE_ORBIT_RADIUS{3.f};
	float m_miniSphereAngle{};

	//// Reflectance
	// Reflector
	uint32_t m_myCoolReflectorPoint{ 0 };
	const TriVector m_REFLECTOR_POS{ 0.5f, 1.f, -30.f };
	// Pheasant mesh
	uint32_t m_pheasantMeshId{ 0 };
	const float m_PHEASANT_SCALE{ 0.5f };
	const float m_PHEASANT_ROTATION_SPEED{ 1.2f };
	TriVector m_PheasantPos{ m_REFLECTOR_POS.e032() + 20.f, m_REFLECTOR_POS.e013(), m_REFLECTOR_POS.e021()};
	
	const float m_PHEASANT_COLLISION_RADIUS{ 3.f };
	

	////////////////////////////////////////////////////////////////////////////// FUNCTIONS
	// update
	void MoveCharacter(float deltaTime);
	void CheckPlayerCollisionWithWalls(TriVector& newPosition);
	bool IsNewPlayerPositionValid(const TriVector& position);

	void UpdateMiniSphere(float deltaTime);

	void UpdateCameraTransform();
	void CheckCameraCollisionWithWalls(TriVector& newCameraPosition);
	bool IsNewCameraPositionValid(const TriVector& position);

	void UpdatePheasant(float deltaTime);
	bool CheckSphereCollision(const TriVector& center1, float radius1, const TriVector& center2, float radius2);
	void ReflectPheasant();

	//input
	InputState m_Input;
	void UpdateCameraOrientation(float mouseDeltaX, float mouseDeltaY);

};
