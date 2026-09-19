#include "MyCoolScene.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <iostream>

MyCoolScene::MyCoolScene(const std::string & resourceDir) :
	GameScene(resourceDir)
{
}

void MyCoolScene::OnInit([[maybe_unused]] VulkanRenderer * renderer)
{
	// Camera
	m_cameraEye = TriVector(0.0f, 15.0f, 60.0f);
	m_cameraTarget = m_PlayerCenter;
	m_cameraTarget.e013() += 5.0f;
	m_cameraUp = TriVector(0.0f, 1.0f, 0.0f, 0.0f);

	// Point light
	AddPointLight(TriVector(0.0f, 20.0f, 0.0f), Scene::Color(1.0f, 1.0f, 1.0f), 2.0f, 100.0f);

	////////////////////////////////////////////////////////////////////////////// ROOM

	// Bottom plane
	AddPlane(Vector(0.0f, 0.0f, 1.0f, 0.0f), Scene::Material::Lambert(Scene::Color(0.6f, 0.6f, 0.6f)));

	// Top plane
	AddPlane(Vector(m_ROOM_HEIGHT, 0.0f, -1.0f, 0.0f), Scene::Material::Lambert(Scene::Color(0.8f, 0.8f, 0.9f)));

	// Z plane
	AddPlane(Vector(m_HALF_LENGHT_ROOM, 0.0f, 0.f, 1.0f), Scene::Material::Lambert(Scene::Color(0.2f, 0.f, 0.2f)));

	// -Z plane
	AddPlane(Vector(m_HALF_LENGHT_ROOM, 0.0f, 0.f, -1.0f), Scene::Material::Lambert(Scene::Color(0.8f, 0.8f, 0.f)));

	// Left plane
	AddPlane(Vector(m_HALF_WIDTH_ROOM, 1.0f, 0.0f, 0.0f), Scene::Material::Lambert(Scene::Color(0.9f, 0.5f, 0.5f)));

	// Right plane
	AddPlane(Vector(m_HALF_WIDTH_ROOM, -1.0f, 0.0f, 0.0f), Scene::Material::Lambert(Scene::Color(0.5f, 0.9f, 0.5f)));

	//////////////////////////////////////////////////////////////////////////////// My character
	const Scene::Color PLAYER_COLOR{ 1.f, 1.f, 0.f };
	const Scene::Material PLAYER_MATERIAL{ Scene::Material::Lambert(PLAYER_COLOR) };
	m_Player = AddSphere(m_PlayerCenter, m_PLAYER_RADIUS, PLAYER_MATERIAL);

	////////////////////////////////////////////////////////////////////////////// Mini Character
	m_miniSphereId = AddSphere( TriVector(m_PlayerCenter.e032() + m_MINI_SPHERE_ORBIT_RADIUS, m_MINI_SPHERE_RADIUS, m_PlayerCenter.e021()),
							 m_MINI_SPHERE_RADIUS,
							 Scene::Material::Metal(Scene::Color(0.9f, 0.3f, 0.3f), 0.2f) );


	//////////////////////////////////////////////////////////////////////////////// Pheasant
	
	m_myCoolReflectorPoint = AddSphere(m_REFLECTOR_POS, 1.f, Scene::Material::Lambert(Scene::Color(0.2f, 0.2f, 0.3f)));

	m_pheasantMeshId = LoadMesh("pheasant.obj", "pheasant.png");
	AddMeshInstance(m_pheasantMeshId, m_PheasantPos, "pheasant");


	if (auto* pheasant = FindInstance("pheasant"))
	{
		pheasant->scale = m_PHEASANT_SCALE;
	}

}

void MyCoolScene::OnUpdate(float deltaTime)
{
	UpdateFPS(deltaTime);

	MoveCharacter(deltaTime);
	UpdateMiniSphere(deltaTime);
	UpdateCameraTransform();
	UpdatePheasant(deltaTime);
}


void MyCoolScene::MoveCharacter(float deltaTime)
{
	float forward{};
	float right{};

	if (m_Input.keyW || m_Input.keyUp)    forward += 1.0f;
	if (m_Input.keyS || m_Input.keyDown)  forward -= 1.0f;
	if (m_Input.keyD || m_Input.keyRight) right -= 1.0f;
	if (m_Input.keyA || m_Input.keyLeft)  right += 1.0f;

	if (forward != 0.f || right != 0.f)
	{
		// Normalize input
		float inputMagnitude{ std::sqrt(forward * forward + right * right) };
		forward /= inputMagnitude;
		right /= inputMagnitude;

		float step{ m_PLAYER_SPEED * deltaTime };

		// Transform movement direction based on camera yaw
		float camForwardX {  std::sin(m_cameraYaw)};
		float camForwardZ {  std::cos(m_cameraYaw)};
		float camRightX   {  std::cos(m_cameraYaw)};
		float camRightZ   { -std::sin(m_cameraYaw)};

		float worldX { camRightX * right + camForwardX * forward };
		float worldZ { camRightZ * right + camForwardZ * forward };

		// The 0.5 factor compensates for the fact that the sandwich product T* X * ~T inherently doubles the translation stored in the motor.
		const float SANDWICH_COMPENSATION{ 0.5f };
		Motor T(1.0f,
				worldX * step * SANDWICH_COMPENSATION,
				0.0f,                 
				worldZ * step * SANDWICH_COMPENSATION,
				0.0f, 0.0f, 0.0f, 0.0f);

		TriVector newPlayerPos = (T * m_PlayerCenter * ~T).Grade3();
		CheckPlayerCollisionWithWalls(newPlayerPos);
	}

	if (auto* player = GetSphere(m_Player))
	{
		player->SetCenter(m_PlayerCenter);
	}
}

void MyCoolScene::CheckPlayerCollisionWithWalls(TriVector& newPosition)
{
	if (IsNewPlayerPositionValid(newPosition))
	{
		m_PlayerCenter = newPosition;
	}
}

bool MyCoolScene::IsNewPlayerPositionValid(const TriVector& newPosition)
{
	// Right wall X (green)
	if (newPosition.e032() > m_HALF_WIDTH_ROOM - m_PLAYER_RADIUS) return false;

	// Left wall -X (red)
	if (newPosition.e032() < -m_HALF_WIDTH_ROOM + m_PLAYER_RADIUS) return false;

	// Z (yellow)
	if (newPosition.e021() > m_HALF_LENGHT_ROOM - m_PLAYER_RADIUS ) return false;

	// -Z (purple)
	if (newPosition.e021() < -m_HALF_LENGHT_ROOM + m_PLAYER_RADIUS) return false;

	return true;
}

void MyCoolScene::UpdateMiniSphere(float deltaTime)
{
	m_miniSphereAngle += m_PHEASANT_ROTATION_SPEED * deltaTime;

	TriVector localOffset = TriVector(m_MINI_SPHERE_ORBIT_RADIUS,
									  m_MINI_SPHERE_RADIUS + 1.f,
									  0.0f);
	const float DEGREES = m_miniSphereAngle * (180.0f / kPi);
	const BiVector Y_AXIS(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	const Motor R = Motor::Rotation(DEGREES, Y_AXIS);

	// Apply rotation to get rotated offset
	TriVector rotatedOffset = (R * localOffset * ~R).Grade3();

	// Add player position to the rotated offset
	TriVector newPos = TriVector(m_PlayerCenter.e032() + rotatedOffset.e032(),
								rotatedOffset.e013(),
								m_PlayerCenter.e021() + rotatedOffset.e021());

	if (auto* sphere = GetSphere(m_miniSphereId))
	{
		sphere->SetCenter(newPos);
	}
}

void MyCoolScene::UpdateCameraTransform()
{
	m_cameraTarget = m_PlayerCenter;
	m_cameraTarget.e013() += 5.0f;

	// Calculate spherical coordinates for orbit camera
	const float camX = std::sin(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;
	const float camY = std::sin(m_cameraPitch) * m_cameraDistance;
	const float camZ = std::cos(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;

	TriVector newCameraEye = TriVector( m_cameraTarget.e032() + camX,
										m_cameraTarget.e013() + camY,
										m_cameraTarget.e021() + camZ);

	CheckCameraCollisionWithWalls(newCameraEye);
}

void MyCoolScene::CheckCameraCollisionWithWalls(TriVector& newCameraPosition)
{
	if (IsNewCameraPositionValid(newCameraPosition))
	{
		m_cameraEye = newCameraPosition;
		m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);
	}
}

bool MyCoolScene::IsNewCameraPositionValid(const TriVector& newPosition)
{
	const float CAM_MARGIN {0.5f};

	// Right wall +X
	if (newPosition.e032() > m_HALF_WIDTH_ROOM - CAM_MARGIN) return false;

	// Left wall -X
	if (newPosition.e032() < -m_HALF_WIDTH_ROOM + CAM_MARGIN) return false;

	// Back wall +Z
	if (newPosition.e021() > m_HALF_LENGHT_ROOM - CAM_MARGIN) return false;

	// Front wall -Z
	if (newPosition.e021() < -m_HALF_LENGHT_ROOM + CAM_MARGIN) return false;

	// Floor
	if (newPosition.e013() < CAM_MARGIN) return false;

	// Ceiling
	if (newPosition.e013() > m_ROOM_HEIGHT - CAM_MARGIN) return false;

	return true;
}


void MyCoolScene::UpdatePheasant(float deltaTime)
{
	if (auto* pheasant = FindInstance("pheasant"))
	{
		const float INCREMENT_ANGLE = m_PHEASANT_ROTATION_SPEED * deltaTime;
		const float INCREMENT_DEGREES = INCREMENT_ANGLE * (180.0f / kPi);
		const BiVector Y_AXIS(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		const Motor R{ Motor::Rotation(INCREMENT_DEGREES, Y_AXIS) };
		const Motor T{ pheasant->transform };

		pheasant->transform = R * T;
	}

	// Check collision between player and pheasant
	if (CheckSphereCollision(m_PlayerCenter, m_PLAYER_RADIUS,
					         m_PheasantPos, m_PHEASANT_COLLISION_RADIUS))
	{
		std::cout << "COLLISION WITH PHEASANT!" << std::endl;
		ReflectPheasant();
	}
}

bool MyCoolScene::CheckSphereCollision(const TriVector& center1, float radius1,
									   const TriVector& center2, float radius2)
{
	float dx = center1.e032() - center2.e032();
	float dy = center1.e013() - center2.e013();
	float dz = center1.e021() - center2.e021();

	float distanceSquared = dx * dx + dy * dy + dz * dz;
	float minDistance = radius1 + radius2;

	return distanceSquared < (minDistance * minDistance);
}

void MyCoolScene::ReflectPheasant()
{
	if (auto* pheasant = FindInstance("pheasant"))
	{
		TriVector reflected = (m_REFLECTOR_POS * m_PheasantPos * ~m_REFLECTOR_POS).Grade3();
		m_PheasantPos = reflected;
		
		const float SANDWICH_COMPENSATION{ 0.5f };
		Motor T(1.0f,
				m_PheasantPos.e032() * SANDWICH_COMPENSATION,
				m_PheasantPos.e013() * SANDWICH_COMPENSATION,
				m_PheasantPos.e021() * SANDWICH_COMPENSATION,
				0.0f, 0.0f, 0.0f, 0.0f);

		pheasant->transform = T;

		std::cout << "Pheasant reflected using PGA sandwich product!" << std::endl;
	}
}

void MyCoolScene::OnInput(const InputState& input)
{
	m_Input = input;

	if (m_Input.leftMouseDown)
	{
		UpdateCameraOrientation(m_Input.mouseDeltaX, m_Input.mouseDeltaY);
	}
}

void MyCoolScene::UpdateCameraOrientation(float mouseDeltaX, float mouseDeltaY)
{
	m_cameraYaw += mouseDeltaX * m_mouseSensitivity;
	m_cameraPitch += mouseDeltaY * m_mouseSensitivity;
	m_cameraPitch = std::clamp(m_cameraPitch, -1.4f, 1.4f);
}

void MyCoolScene::OnGui()
{
}

void MyCoolScene::OnShutdown()
{
}
