#include "Mesh.h"
#include <tiny_obj_loader.h>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <filesystem>
#include <queue>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <functional>

bool Mesh::LoadFromFile(const std::string& filename) {
    // Extract the directory for material file lookup
    std::filesystem::path filePath(filename);
    std::string mtlBasePath = filePath.parent_path().string();
    if (!mtlBasePath.empty()) {
        mtlBasePath += "/";
    }
    return LoadFromFile(filename, mtlBasePath);
}

bool Mesh::LoadFromFile(const std::string& objFilename, const std::string& mtlBasePath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> objMaterials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &objMaterials, &warn, &err,
                          objFilename.c_str(), mtlBasePath.c_str())) {
        std::cerr << "Failed to load " << objFilename << ": " << err << std::endl;
        return false;
    }

    if (!warn.empty()) {
        std::cout << "Warning loading " << objFilename << ": " << warn << std::endl;
    }

    // Clear existing data
    Clear();

    // Convert tinyobj materials to our Material format
    for (const auto& mat : objMaterials) {
        Material material;
        material.name = mat.name;

        // Ambient
        material.ambient[0] = mat.ambient[0];
        material.ambient[1] = mat.ambient[1];
        material.ambient[2] = mat.ambient[2];

        // Diffuse
        material.diffuse[0] = mat.diffuse[0];
        material.diffuse[1] = mat.diffuse[1];
        material.diffuse[2] = mat.diffuse[2];

        // Specular
        material.specular[0] = mat.specular[0];
        material.specular[1] = mat.specular[1];
        material.specular[2] = mat.specular[2];

        // Shininess
        material.shininess = mat.shininess;

        // Emission
        material.emission[0] = mat.emission[0];
        material.emission[1] = mat.emission[1];
        material.emission[2] = mat.emission[2];

        // Opacity (dissolve in OBJ)
        material.opacity = mat.dissolve;

        // Texture paths
        if (!mat.diffuse_texname.empty()) {
            material.diffuseTexturePath = mtlBasePath + mat.diffuse_texname;
        }
        if (!mat.specular_texname.empty()) {
            material.specularTexturePath = mtlBasePath + mat.specular_texname;
        }

        m_materials.push_back(material);
    }

    // Add a default material if none were loaded
    if (m_materials.empty()) {
        Material defaultMaterial;
        defaultMaterial.name = "default";
        m_materials.push_back(defaultMaterial);
    }

    // Use a map to deduplicate vertices
    std::unordered_map<size_t, uint32_t> vertexMap;
    auto hashVertex = [](const Vertex& v) -> size_t {
        size_t h1 = std::hash<float>{}(v.position.e032());
        size_t h2 = std::hash<float>{}(v.position.e013());
        size_t h3 = std::hash<float>{}(v.position.e021());
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    };

    // Process all shapes
    for (const auto& shape : shapes) {
        size_t indexOffset = 0;

        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            size_t fv = shape.mesh.num_face_vertices[f];

            // Only support triangles
            if (fv != 3) {
                std::cerr << "Warning: Skipping non-triangle face" << std::endl;
                indexOffset += fv;
                continue;
            }

            Triangle triangle{};

            // Get material index for this face
            int matId = shape.mesh.material_ids[f];
            triangle.materialIndex = (matId >= 0) ? static_cast<uint32_t>(matId) : 0;

            for (size_t v = 0; v < 3; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                Vertex vertex{};

                // Position as TriVector (point in PGA: x=e032, y=e013, z=e021, w=e123=1)
                if (idx.vertex_index >= 0) {
                    vertex.position = TriVector(
                        attrib.vertices[3 * idx.vertex_index + 0],
                        attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2]
                    );
                }

                // Normal as Vector (plane in PGA: e0=0, e1=nx, e2=ny, e3=nz)
                // Check both that we have a valid index AND that normals actually exist in the file
                // Some OBJ files reference normals (f v/vt/vn) without defining them (no vn lines)
                if (idx.normal_index >= 0 && !attrib.normals.empty() &&
                    static_cast<size_t>(idx.normal_index * 3 + 2) < attrib.normals.size()) {
                    vertex.normal = Vector(
                        0.0f,
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    );
                } else {
                    // Will compute later if needed
                    vertex.normal = Vector(0.0f, 0.0f, 0.0f, 0.0f);
                }

                // Texture coordinates (UV)
                // OBJ uses V=0 at bottom, but stb_image loads with row 0 at top
                // Flip V coordinate: 1.0 - v
                if (idx.texcoord_index >= 0) {
                    vertex.texCoord[0] = attrib.texcoords[2 * idx.texcoord_index + 0];
                    vertex.texCoord[1] = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];
                } else {
                    vertex.texCoord[0] = 0.0f;
                    vertex.texCoord[1] = 0.0f;
                }

                // Check if vertex already exists
                size_t hash = hashVertex(vertex);
                auto it = vertexMap.find(hash);

                uint32_t vertexIndex;
                if (it != vertexMap.end()) {
                    vertexIndex = it->second;
                } else {
                    vertexIndex = static_cast<uint32_t>(m_vertices.size());
                    m_vertices.push_back(vertex);
                    vertexMap[hash] = vertexIndex;
                }

                triangle.indices[v] = vertexIndex;
            }

            m_triangles.push_back(triangle);
            indexOffset += fv;
        }
    }

    // Compute normals if they weren't provided
    // Use a small epsilon for float comparison since exact zero check is unreliable
    constexpr float epsilon = 1e-6f;
    bool needsNormals = false;
    for (const auto& v : m_vertices) {
        // Normal is stored as Vector with e1=nx, e2=ny, e3=nz
        const float lenSq = v.normal.e1() * v.normal.e1() +
                            v.normal.e2() * v.normal.e2() +
                            v.normal.e3() * v.normal.e3();
        if (lenSq < epsilon) {
            needsNormals = true;
            break;
        }
    }
    if (needsNormals) {
        ComputeNormals();
    }

    return !m_vertices.empty();
}

void Mesh::Clear() {
    m_vertices.clear();
    m_triangles.clear();
    m_materials.clear();
}

void Mesh::ClearCPUData() {
    // Free CPU memory but keep BVH, metadata, and physics data
    // This is called after GPU upload to save memory
    m_vertices.clear();
    m_vertices.shrink_to_fit();
    m_triangles.clear();
    m_triangles.shrink_to_fit();
    // Keep materials as they're small and may be needed for updates
    // Keep physics data (bounding box, face normals) for collision detection
}

void Mesh::AddMaterial(const Material& material) {
    m_materials.push_back(material);
}

const Material& Mesh::GetMaterial(size_t index) const {
    if (index >= m_materials.size()) {
        throw std::out_of_range("Material index " + std::to_string(index) +
                                " out of range (size: " + std::to_string(m_materials.size()) + ")");
    }
    return m_materials[index];
}

Material& Mesh::GetMaterial(size_t index) {
    if (index >= m_materials.size()) {
        throw std::out_of_range("Material index " + std::to_string(index) +
                                " out of range (size: " + std::to_string(m_materials.size()) + ")");
    }
    return m_materials[index];
}

void Mesh::AddVertex(const Vertex& vertex) {
    m_vertices.push_back(vertex);
}

void Mesh::AddTriangle(const Triangle& triangle) {
    m_triangles.push_back(triangle);
}

void Mesh::SetVertices(std::vector<Vertex>&& vertices) {
    m_vertices = std::move(vertices);
}

void Mesh::SetTriangles(std::vector<Triangle>&& triangles) {
    m_triangles = std::move(triangles);
}

void Mesh::ComputeNormals() {
    // Reset all normals to zero (Vector: e0=0, e1=nx, e2=ny, e3=nz)
    for (auto& v : m_vertices) {
        v.normal = Vector(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Compute face normals and accumulate to vertices using angle-weighting
    for (const auto& tri : m_triangles) {
        const Vertex& v0 = m_vertices[tri.indices[0]];
        const Vertex& v1 = m_vertices[tri.indices[1]];
        const Vertex& v2 = m_vertices[tri.indices[2]];

        // Get positions from TriVectors (e032=x, e013=y, e021=z)
        float p0[3] = {v0.position.e032(), v0.position.e013(), v0.position.e021()};
        float p1[3] = {v1.position.e032(), v1.position.e013(), v1.position.e021()};
        float p2[3] = {v2.position.e032(), v2.position.e013(), v2.position.e021()};

        // Edge vectors from v0
        float e01[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        float e02[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

        // Cross product for face normal (wedge product in GA)
        // In GA: cross product is the Hodge dual of the wedge product e01 ∧ e02
        // The wedge product creates a bivector (oriented area element)
        // The dual maps it to a vector perpendicular to the plane
        // Note: We negate the result because OBJ files typically use clockwise
        // winding order when viewed from outside, so e01 × e02 points inward.
        // Negating gives us outward-facing normals for correct lighting.
        float nx = -(e01[1] * e02[2] - e01[2] * e02[1]);
        float ny = -(e01[2] * e02[0] - e01[0] * e02[2]);
        float nz = -(e01[0] * e02[1] - e01[1] * e02[0]);

        // Normalize face normal
        float faceLen = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (faceLen < 0.0001f) continue;
        nx /= faceLen;
        ny /= faceLen;
        nz /= faceLen;

        // Compute angle at each vertex and weight the normal contribution
        // Edge vectors for angle calculation
        float e10[3] = {p0[0] - p1[0], p0[1] - p1[1], p0[2] - p1[2]};
        float e12[3] = {p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2]};
        float e20[3] = {p0[0] - p2[0], p0[1] - p2[1], p0[2] - p2[2]};
        float e21[3] = {p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]};

        auto dot3 = [](float a[3], float b[3]) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; };
        auto len3 = [](float a[3]) { return std::sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]); };

        // Angle at v0
        float len01 = len3(e01);
        float len02 = len3(e02);
        float angle0 = (len01 > 0.0001f && len02 > 0.0001f) ?
                       std::acos(std::clamp(dot3(e01, e02) / (len01 * len02), -1.0f, 1.0f)) : 0.0f;

        // Angle at v1
        float len10 = len3(e10);
        float len12 = len3(e12);
        float angle1 = (len10 > 0.0001f && len12 > 0.0001f) ?
                       std::acos(std::clamp(dot3(e10, e12) / (len10 * len12), -1.0f, 1.0f)) : 0.0f;

        // Angle at v2
        float len20 = len3(e20);
        float len21 = len3(e21);
        float angle2 = (len20 > 0.0001f && len21 > 0.0001f) ?
                       std::acos(std::clamp(dot3(e20, e21) / (len20 * len21), -1.0f, 1.0f)) : 0.0f;

        // Accumulate weighted normals (using e1, e2, e3 for nx, ny, nz)
        m_vertices[tri.indices[0]].normal.e1() += nx * angle0;
        m_vertices[tri.indices[0]].normal.e2() += ny * angle0;
        m_vertices[tri.indices[0]].normal.e3() += nz * angle0;

        m_vertices[tri.indices[1]].normal.e1() += nx * angle1;
        m_vertices[tri.indices[1]].normal.e2() += ny * angle1;
        m_vertices[tri.indices[1]].normal.e3() += nz * angle1;

        m_vertices[tri.indices[2]].normal.e1() += nx * angle2;
        m_vertices[tri.indices[2]].normal.e2() += ny * angle2;
        m_vertices[tri.indices[2]].normal.e3() += nz * angle2;
    }

    // Normalize all vertex normals
    for (auto& v : m_vertices) {
        float len = std::sqrt(v.normal.e1() * v.normal.e1() +
                              v.normal.e2() * v.normal.e2() +
                              v.normal.e3() * v.normal.e3());
        if (len > 0.0001f) {
            v.normal.e1() /= len;
            v.normal.e2() /= len;
            v.normal.e3() /= len;
        } else {
            // Default up normal (e0=0, e1=0, e2=1, e3=0)
            v.normal = Vector(0.0f, 0.0f, 1.0f, 0.0f);
        }
    }
}

void Mesh::Scale(float factor) {
    for (auto& v : m_vertices) {
        // Scale position (TriVector: e032=x, e013=y, e021=z)
        v.position.e032() *= factor;
        v.position.e013() *= factor;
        v.position.e021() *= factor;
    }
}

void Mesh::Translate(float x, float y, float z) {
    for (auto& v : m_vertices) {
        // Translate position (TriVector: e032=x, e013=y, e021=z)
        v.position.e032() += x;
        v.position.e013() += y;
        v.position.e021() += z;
    }
}

void Mesh::CenterOnOrigin() {
    if (m_vertices.empty()) return;

    // Find bounding box using TriVector components
    float minX = m_vertices[0].position.e032(), maxX = minX;
    float minY = m_vertices[0].position.e013(), maxY = minY;
    float minZ = m_vertices[0].position.e021(), maxZ = minZ;

    for (const auto& v : m_vertices) {
        minX = std::min(minX, v.position.e032());
        maxX = std::max(maxX, v.position.e032());
        minY = std::min(minY, v.position.e013());
        maxY = std::max(maxY, v.position.e013());
        minZ = std::min(minZ, v.position.e021());
        maxZ = std::max(maxZ, v.position.e021());
    }

    // Compute center
    float centerX = (minX + maxX) / 2.0f;
    float centerY = minY; // Keep bottom at y=0
    float centerZ = (minZ + maxZ) / 2.0f;

    // Translate to center
    Translate(-centerX, -centerY, -centerZ);
}

void Mesh::Decimate(float targetRatio) {
    if (m_triangles.empty() || targetRatio >= 1.0f) return;

    targetRatio = std::max(0.01f, std::min(1.0f, targetRatio));
    size_t targetTriCount = static_cast<size_t>(m_triangles.size() * targetRatio);
    targetTriCount = std::max(targetTriCount, size_t(4)); // Keep at least 4 triangles

    std::cout << "Decimating mesh from " << m_triangles.size() << " to ~"
              << targetTriCount << " triangles..." << std::endl;

    const size_t numVertices = m_vertices.size();
    const size_t numTriangles = m_triangles.size();

    // ========== Union-Find with path compression ==========
    std::vector<uint32_t> parent(numVertices);
    for (size_t i = 0; i < numVertices; ++i) {
        parent[i] = static_cast<uint32_t>(i);
    }

    // Find with path compression - flattens tree for O(α(n)) amortized
    std::function<uint32_t(uint32_t)> findRoot = [&parent, &findRoot](uint32_t v) -> uint32_t {
        if (parent[v] != v) {
            parent[v] = findRoot(parent[v]); // Path compression
        }
        return parent[v];
    };

    // ========== Vertex-to-triangle adjacency ==========
    // This allows O(degree) updates instead of O(n) full scans
    std::vector<std::vector<uint32_t>> vertexTriangles(numVertices);
    for (size_t ti = 0; ti < numTriangles; ++ti) {
        const Triangle& tri = m_triangles[ti];
        vertexTriangles[tri.indices[0]].push_back(static_cast<uint32_t>(ti));
        vertexTriangles[tri.indices[1]].push_back(static_cast<uint32_t>(ti));
        vertexTriangles[tri.indices[2]].push_back(static_cast<uint32_t>(ti));
    }

    // Track which triangles are still valid
    std::vector<bool> triangleValid(numTriangles, true);
    size_t validTriCount = numTriangles;

    // Copy vertex positions (modified during collapse)
    // Using GA types: TriVector for position (e032=x, e013=y, e021=z), Vector for normal (e1=nx, e2=ny, e3=nz)
    std::vector<std::array<float, 3>> positions(numVertices);
    std::vector<std::array<float, 3>> normals(numVertices);
    for (size_t i = 0; i < numVertices; ++i) {
        positions[i] = {m_vertices[i].position.e032(), m_vertices[i].position.e013(), m_vertices[i].position.e021()};
        normals[i] = {m_vertices[i].normal.e1(), m_vertices[i].normal.e2(), m_vertices[i].normal.e3()};
    }

    // Edge key packing
    auto makeEdgeKey = [](uint32_t v1, uint32_t v2) -> uint64_t {
        if (v1 > v2) std::swap(v1, v2);
        return (static_cast<uint64_t>(v1) << 32) | static_cast<uint64_t>(v2);
    };

    // Calculate edge collapse cost (edge length)
    auto calcEdgeCost = [&positions](uint32_t v1, uint32_t v2) -> float {
        float dx = positions[v1][0] - positions[v2][0];
        float dy = positions[v1][1] - positions[v2][1];
        float dz = positions[v1][2] - positions[v2][2];
        return dx*dx + dy*dy + dz*dz; // Squared distance (avoid sqrt in inner loop)
    };

    // Priority queue: (cost, v1, v2)
    using EdgeEntry = std::tuple<float, uint32_t, uint32_t>;
    std::priority_queue<EdgeEntry, std::vector<EdgeEntry>, std::greater<EdgeEntry>> edgeQueue;

    // Build initial edge queue (deduplicated)
    std::unordered_set<uint64_t> addedEdges;
    addedEdges.reserve(numTriangles * 3);
    for (size_t ti = 0; ti < numTriangles; ++ti) {
        const Triangle& tri = m_triangles[ti];
        for (int e = 0; e < 3; ++e) {
            uint32_t v1 = tri.indices[e];
            uint32_t v2 = tri.indices[(e + 1) % 3];
            uint64_t edgeKey = makeEdgeKey(v1, v2);
            if (addedEdges.insert(edgeKey).second) {
                float cost = calcEdgeCost(v1, v2);
                edgeQueue.push({cost, v1, v2});
            }
        }
    }
    addedEdges.clear(); // Free memory, no longer needed

    // Working triangle indices (avoid repeated allocation)
    std::vector<uint32_t> affectedTriangles;
    affectedTriangles.reserve(64);

    // Edge collapse loop
    while (validTriCount > targetTriCount && !edgeQueue.empty()) {
        auto [cost, origV1, origV2] = edgeQueue.top();
        edgeQueue.pop();

        // Find current roots (with path compression)
        uint32_t v1 = findRoot(origV1);
        uint32_t v2 = findRoot(origV2);

        // Skip if already merged
        if (v1 == v2) continue;

        // Ensure v1 < v2 for consistent merging
        if (v1 > v2) std::swap(v1, v2);

        // Recalculate cost - skip if significantly changed (lazy update)
        float newCost = calcEdgeCost(v1, v2);
        if (newCost > cost * 2.25f) { // 1.5^2 since we use squared distance
            edgeQueue.push({newCost, v1, v2});
            continue;
        }

        // ========== Collapse edge: merge v2 into v1 ==========
        // Update position to midpoint
        positions[v1][0] = (positions[v1][0] + positions[v2][0]) * 0.5f;
        positions[v1][1] = (positions[v1][1] + positions[v2][1]) * 0.5f;
        positions[v1][2] = (positions[v1][2] + positions[v2][2]) * 0.5f;

        // Average and normalize normals
        normals[v1][0] = normals[v1][0] + normals[v2][0];
        normals[v1][1] = normals[v1][1] + normals[v2][1];
        normals[v1][2] = normals[v1][2] + normals[v2][2];
        float nlen = std::sqrt(normals[v1][0]*normals[v1][0] +
                               normals[v1][1]*normals[v1][1] +
                               normals[v1][2]*normals[v1][2]);
        if (nlen > 1e-6f) {
            normals[v1][0] /= nlen;
            normals[v1][1] /= nlen;
            normals[v1][2] /= nlen;
        }

        // Union: v2 -> v1
        parent[v2] = v1;

        // ========== Update only affected triangles ==========
        // Collect triangles touching v1 or v2
        affectedTriangles.clear();
        for (uint32_t ti : vertexTriangles[v1]) {
            if (triangleValid[ti]) affectedTriangles.push_back(ti);
        }
        for (uint32_t ti : vertexTriangles[v2]) {
            if (triangleValid[ti]) affectedTriangles.push_back(ti);
        }

        // Merge v2's triangle list into v1's
        vertexTriangles[v1].insert(vertexTriangles[v1].end(),
                                   vertexTriangles[v2].begin(),
                                   vertexTriangles[v2].end());
        vertexTriangles[v2].clear();
        vertexTriangles[v2].shrink_to_fit();

        // Update affected triangles
        for (uint32_t ti : affectedTriangles) {
            if (!triangleValid[ti]) continue;

            Triangle& tri = m_triangles[ti];

            // Remap all vertices to their roots
            tri.indices[0] = findRoot(tri.indices[0]);
            tri.indices[1] = findRoot(tri.indices[1]);
            tri.indices[2] = findRoot(tri.indices[2]);

            // Check for degenerate triangle
            if (tri.indices[0] == tri.indices[1] ||
                tri.indices[1] == tri.indices[2] ||
                tri.indices[2] == tri.indices[0]) {
                triangleValid[ti] = false;
                --validTriCount;
            } else {
                // Add new edges involving v1 to queue
                for (int e = 0; e < 3; ++e) {
                    uint32_t ev1 = tri.indices[e];
                    uint32_t ev2 = tri.indices[(e + 1) % 3];
                    if (ev1 == v1 || ev2 == v1) {
                        float edgeCost = calcEdgeCost(ev1, ev2);
                        edgeQueue.push({edgeCost, ev1, ev2});
                    }
                }
            }
        }
    }

    // ========== Rebuild mesh with valid triangles ==========
    std::unordered_map<uint32_t, uint32_t> usedVertices;
    usedVertices.reserve(numVertices / 4);
    std::vector<Vertex> newVertices;
    newVertices.reserve(numVertices / 4);
    std::vector<Triangle> newTriangles;
    newTriangles.reserve(validTriCount);

    for (size_t ti = 0; ti < numTriangles; ++ti) {
        if (!triangleValid[ti]) continue;

        Triangle newTri;
        newTri.materialIndex = m_triangles[ti].materialIndex;

        for (int i = 0; i < 3; ++i) {
            uint32_t oldIdx = findRoot(m_triangles[ti].indices[i]);

            auto it = usedVertices.find(oldIdx);
            if (it == usedVertices.end()) {
                uint32_t newIdx = static_cast<uint32_t>(newVertices.size());
                usedVertices[oldIdx] = newIdx;

                Vertex v;
                // Rebuild TriVector position (x=e032, y=e013, z=e021, w=e123=1)
                v.position = TriVector(positions[oldIdx][0], positions[oldIdx][1], positions[oldIdx][2]);
                // Rebuild Vector normal (e0=0, e1=nx, e2=ny, e3=nz)
                v.normal = Vector(0.0f, normals[oldIdx][0], normals[oldIdx][1], normals[oldIdx][2]);
                v.texCoord[0] = m_vertices[oldIdx].texCoord[0];
                v.texCoord[1] = m_vertices[oldIdx].texCoord[1];
                newVertices.push_back(v);

                newTri.indices[i] = newIdx;
            } else {
                newTri.indices[i] = it->second;
            }
        }

        // Final degenerate check
        if (newTri.indices[0] != newTri.indices[1] &&
            newTri.indices[1] != newTri.indices[2] &&
            newTri.indices[2] != newTri.indices[0]) {
            newTriangles.push_back(newTri);
        }
    }

    m_vertices = std::move(newVertices);
    m_triangles = std::move(newTriangles);

    // Recompute normals for smooth shading
    ComputeNormals();

    std::cout << "Decimation complete: " << m_vertices.size() << " vertices, "
              << m_triangles.size() << " triangles" << std::endl;
}

void Mesh::ComputePhysicsData() {
    if (m_vertices.empty() || m_triangles.empty()) {
        m_hasPhysicsData = false;
        return;
    }

    // Compute bounding box using TriVector components (e032=x, e013=y, e021=z)
    m_boundingBox.min[0] = m_vertices[0].position.e032();
    m_boundingBox.min[1] = m_vertices[0].position.e013();
    m_boundingBox.min[2] = m_vertices[0].position.e021();
    m_boundingBox.max[0] = m_vertices[0].position.e032();
    m_boundingBox.max[1] = m_vertices[0].position.e013();
    m_boundingBox.max[2] = m_vertices[0].position.e021();

    for (const auto& v : m_vertices) {
        m_boundingBox.min[0] = std::min(m_boundingBox.min[0], v.position.e032());
        m_boundingBox.min[1] = std::min(m_boundingBox.min[1], v.position.e013());
        m_boundingBox.min[2] = std::min(m_boundingBox.min[2], v.position.e021());
        m_boundingBox.max[0] = std::max(m_boundingBox.max[0], v.position.e032());
        m_boundingBox.max[1] = std::max(m_boundingBox.max[1], v.position.e013());
        m_boundingBox.max[2] = std::max(m_boundingBox.max[2], v.position.e021());
    }

    // Compute face normals for each triangle
    m_faceNormals.clear();
    m_faceNormals.reserve(m_triangles.size());

    for (const auto& tri : m_triangles) {
        const Vertex& v0 = m_vertices[tri.indices[0]];
        const Vertex& v1 = m_vertices[tri.indices[1]];
        const Vertex& v2 = m_vertices[tri.indices[2]];

        // Get positions from TriVectors (e032=x, e013=y, e021=z)
        float p0[3] = {v0.position.e032(), v0.position.e013(), v0.position.e021()};
        float p1[3] = {v1.position.e032(), v1.position.e013(), v1.position.e021()};
        float p2[3] = {v2.position.e032(), v2.position.e013(), v2.position.e021()};

        // Edge vectors
        float e01[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        float e02[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

        // Cross product for face normal (wedge product in GA)
        FaceNormal faceNormal;
        faceNormal.normal[0] = e01[1] * e02[2] - e01[2] * e02[1];
        faceNormal.normal[1] = e01[2] * e02[0] - e01[0] * e02[2];
        faceNormal.normal[2] = e01[0] * e02[1] - e01[1] * e02[0];

        // Normalize
        float len = std::sqrt(faceNormal.normal[0] * faceNormal.normal[0] +
                              faceNormal.normal[1] * faceNormal.normal[1] +
                              faceNormal.normal[2] * faceNormal.normal[2]);
        if (len > 1e-6f) {
            faceNormal.normal[0] /= len;
            faceNormal.normal[1] /= len;
            faceNormal.normal[2] /= len;
        } else {
            // Degenerate triangle - use default up normal
            faceNormal.normal[0] = 0.0f;
            faceNormal.normal[1] = 1.0f;
            faceNormal.normal[2] = 0.0f;
        }

        // Store vertex indices
        faceNormal.indices[0] = tri.indices[0];
        faceNormal.indices[1] = tri.indices[1];
        faceNormal.indices[2] = tri.indices[2];

        m_faceNormals.push_back(faceNormal);
    }

    m_hasPhysicsData = true;

    std::cout << "Physics data computed: bounding box ["
              << m_boundingBox.min[0] << ", " << m_boundingBox.min[1] << ", " << m_boundingBox.min[2]
              << "] to ["
              << m_boundingBox.max[0] << ", " << m_boundingBox.max[1] << ", " << m_boundingBox.max[2]
              << "], " << m_faceNormals.size() << " face normals" << std::endl;
}

// BVH Implementation using Surface Area Heuristic (SAH)

void Mesh::BuildBVH() {
    if (m_triangles.empty()) return;

    const size_t triCount = m_triangles.size();

    // Initialize triangle indices (will be reordered during build)
    m_bvhTriIndices.resize(triCount);
    for (size_t i = 0; i < triCount; ++i) {
        m_bvhTriIndices[i] = static_cast<uint32_t>(i);
    }

    // Precompute triangle centroids (using TriVector: e032=x, e013=y, e021=z)
    m_triCentroids.resize(triCount * 3);
    size_t degenerateCount = 0;
    for (size_t i = 0; i < triCount; ++i) {
        const Triangle& tri = m_triangles[i];
        const Vertex& v0 = m_vertices[tri.indices[0]];
        const Vertex& v1 = m_vertices[tri.indices[1]];
        const Vertex& v2 = m_vertices[tri.indices[2]];

        // Check for degenerate triangle (vertices too close together)
        float dx1 = v1.position.e032() - v0.position.e032();
        float dy1 = v1.position.e013() - v0.position.e013();
        float dz1 = v1.position.e021() - v0.position.e021();
        float dx2 = v2.position.e032() - v0.position.e032();
        float dy2 = v2.position.e013() - v0.position.e013();
        float dz2 = v2.position.e021() - v0.position.e021();

        // Compute cross product magnitude (2x triangle area)
        float crossX = dy1 * dz2 - dz1 * dy2;
        float crossY = dz1 * dx2 - dx1 * dz2;
        float crossZ = dx1 * dy2 - dy1 * dx2;
        float area = std::sqrt(crossX*crossX + crossY*crossY + crossZ*crossZ);

        if (area < 1e-6f) {
            degenerateCount++;
        }

        m_triCentroids[i * 3 + 0] = (v0.position.e032() + v1.position.e032() + v2.position.e032()) / 3.0f;
        m_triCentroids[i * 3 + 1] = (v0.position.e013() + v1.position.e013() + v2.position.e013()) / 3.0f;
        m_triCentroids[i * 3 + 2] = (v0.position.e021() + v1.position.e021() + v2.position.e021()) / 3.0f;
    }

    if (degenerateCount > 0) {
        std::cerr << "WARNING: Found " << degenerateCount << " degenerate triangles (zero area)" << std::endl;
    }

    // Allocate nodes (worst case: 2N-1 nodes for N triangles)
    m_bvhNodes.clear();
    m_bvhNodes.reserve(triCount * 2);

    // Create root node
    m_bvhNodes.emplace_back();
    m_bvhNodes[0].leftFirst = 0;
    m_bvhNodes[0].triCount = static_cast<int32_t>(triCount);

    updateNodeBounds(0);
    subdivideNode(0);

    // Clear temporary data
    m_triCentroids.clear();
    m_triCentroids.shrink_to_fit();
}

void Mesh::updateNodeBounds(uint32_t nodeIdx) {
    Scene::BVHNode& node = m_bvhNodes[nodeIdx];
    node.minBounds[0] = node.minBounds[1] = node.minBounds[2] = 1e30f;
    node.maxBounds[0] = node.maxBounds[1] = node.maxBounds[2] = -1e30f;

    for (int32_t i = 0; i < node.triCount; ++i) {
        uint32_t triIdx = m_bvhTriIndices[node.leftFirst + i];
        const Triangle& tri = m_triangles[triIdx];

        for (int j = 0; j < 3; ++j) {
            const Vertex& v = m_vertices[tri.indices[j]];
            // Use TriVector components (e032=x, e013=y, e021=z)
            node.minBounds[0] = std::min(node.minBounds[0], v.position.e032());
            node.minBounds[1] = std::min(node.minBounds[1], v.position.e013());
            node.minBounds[2] = std::min(node.minBounds[2], v.position.e021());
            node.maxBounds[0] = std::max(node.maxBounds[0], v.position.e032());
            node.maxBounds[1] = std::max(node.maxBounds[1], v.position.e013());
            node.maxBounds[2] = std::max(node.maxBounds[2], v.position.e021());
        }
    }
}

float Mesh::evaluateSAH(uint32_t nodeIdx, int axis, float pos) {
    const Scene::BVHNode& node = m_bvhNodes[nodeIdx];

    float leftMin[3] = {1e30f, 1e30f, 1e30f};
    float leftMax[3] = {-1e30f, -1e30f, -1e30f};
    float rightMin[3] = {1e30f, 1e30f, 1e30f};
    float rightMax[3] = {-1e30f, -1e30f, -1e30f};
    int leftCount = 0, rightCount = 0;

    for (int32_t i = 0; i < node.triCount; ++i) {
        uint32_t triIdx = m_bvhTriIndices[node.leftFirst + i];
        float centroid = m_triCentroids[triIdx * 3 + axis];

        const Triangle& tri = m_triangles[triIdx];

        if (centroid < pos) {
            leftCount++;
            for (int j = 0; j < 3; ++j) {
                const Vertex& v = m_vertices[tri.indices[j]];
                // Use TriVector components (e032=x, e013=y, e021=z)
                leftMin[0] = std::min(leftMin[0], v.position.e032());
                leftMin[1] = std::min(leftMin[1], v.position.e013());
                leftMin[2] = std::min(leftMin[2], v.position.e021());
                leftMax[0] = std::max(leftMax[0], v.position.e032());
                leftMax[1] = std::max(leftMax[1], v.position.e013());
                leftMax[2] = std::max(leftMax[2], v.position.e021());
            }
        } else {
            rightCount++;
            for (int j = 0; j < 3; ++j) {
                const Vertex& v = m_vertices[tri.indices[j]];
                // Use TriVector components (e032=x, e013=y, e021=z)
                rightMin[0] = std::min(rightMin[0], v.position.e032());
                rightMin[1] = std::min(rightMin[1], v.position.e013());
                rightMin[2] = std::min(rightMin[2], v.position.e021());
                rightMax[0] = std::max(rightMax[0], v.position.e032());
                rightMax[1] = std::max(rightMax[1], v.position.e013());
                rightMax[2] = std::max(rightMax[2], v.position.e021());
            }
        }
    }

    if (leftCount == 0 || rightCount == 0) return 1e30f;

    // Compute surface areas
    float leftExtent[3] = {leftMax[0] - leftMin[0], leftMax[1] - leftMin[1], leftMax[2] - leftMin[2]};
    float rightExtent[3] = {rightMax[0] - rightMin[0], rightMax[1] - rightMin[1], rightMax[2] - rightMin[2]};

    float leftArea = leftExtent[0] * leftExtent[1] + leftExtent[1] * leftExtent[2] + leftExtent[2] * leftExtent[0];
    float rightArea = rightExtent[0] * rightExtent[1] + rightExtent[1] * rightExtent[2] + rightExtent[2] * rightExtent[0];

    return static_cast<float>(leftCount) * leftArea + static_cast<float>(rightCount) * rightArea;
}

void Mesh::subdivideNode(uint32_t nodeIdx) {
    Scene::BVHNode& node = m_bvhNodes[nodeIdx];

    // Don't subdivide small nodes
    if (node.triCount <= 4) return;

    // Find best split using SAH with binned evaluation
    float extent[3] = {
        node.maxBounds[0] - node.minBounds[0],
        node.maxBounds[1] - node.minBounds[1],
        node.maxBounds[2] - node.minBounds[2]
    };

    int bestAxis = 0;
    float bestPos = 0;
    float bestCost = 1e30f;

    // Try each axis
    for (int axis = 0; axis < 3; ++axis) {
        if (extent[axis] < 1e-6f) continue;

        // Use 8 bins for SAH evaluation
        constexpr int NUM_BINS = 8;
        for (int b = 1; b < NUM_BINS; ++b) {
            float pos = node.minBounds[axis] + extent[axis] * static_cast<float>(b) / NUM_BINS;
            float cost = evaluateSAH(nodeIdx, axis, pos);
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestPos = pos;
            }
        }
    }

    // Check if split is beneficial
    float parentArea = extent[0] * extent[1] + extent[1] * extent[2] + extent[2] * extent[0];
    float noSplitCost = static_cast<float>(node.triCount) * parentArea;

    if (bestCost >= noSplitCost) return;

    // Partition triangles
    int32_t i = node.leftFirst;
    int32_t j = i + node.triCount - 1;

    while (i <= j) {
        uint32_t triIdx = m_bvhTriIndices[i];
        float centroid = m_triCentroids[triIdx * 3 + bestAxis];
        if (centroid < bestPos) {
            i++;
        } else {
            std::swap(m_bvhTriIndices[i], m_bvhTriIndices[j]);
            j--;
        }
    }

    int32_t leftCount = i - node.leftFirst;
    if (leftCount == 0 || leftCount == node.triCount) return;

    // Create child nodes
    uint32_t leftChildIdx = static_cast<uint32_t>(m_bvhNodes.size());
    m_bvhNodes.emplace_back();
    m_bvhNodes.emplace_back();

    m_bvhNodes[leftChildIdx].leftFirst = node.leftFirst;
    m_bvhNodes[leftChildIdx].triCount = leftCount;
    m_bvhNodes[leftChildIdx + 1].leftFirst = i;
    m_bvhNodes[leftChildIdx + 1].triCount = node.triCount - leftCount;

    // Convert current node to internal node
    node.leftFirst = static_cast<int32_t>(leftChildIdx);
    node.triCount = 0;

    // Update bounds and recurse
    updateNodeBounds(leftChildIdx);
    updateNodeBounds(leftChildIdx + 1);
    subdivideNode(leftChildIdx);
    subdivideNode(leftChildIdx + 1);
}
