#version 410 core

// Clip each source triangle like buildScreenTriangles() on the CPU, then emit
// candidate fragments for every triangle in the clipped polygon's fan.
layout(triangles) in;
layout(triangle_strip, max_vertices = 21) out;

in VertexData {
    vec4 clip;
    vec3 worldPosition;
    vec3 normal;
    vec2 uv;
} vertexData[];

// The fragment shader evaluates the screen-space edges itself. These values
// must stay constant across each generated candidate triangle.
flat out vec4 gScreenAndInvW[3];
flat out vec3 gWorld[3];
flat out vec3 gNormal[3];
flat out vec2 gUv[3];

uniform vec2 uExtent;

struct ClipVertex {
    vec4 clip;
    vec3 worldPosition;
    vec3 normal;
    vec2 uv;
};

struct ScreenVertex {
    // xy is pixel position, z is depth, and w is 1 / clip-space w.
    vec4 screenAndInvW;
    vec3 worldPosition;
    vec3 normal;
    vec2 uv;
};

// A triangle clipped by six planes has at most nine vertices. Keep the side
// where dot(plane, vertex.clip) >= 0, just as the CPU clipAgainst() does.
int clipAgainst(inout ClipVertex polygon[9], int count, vec4 plane) {
    // TODO: Clip the polygon against this plane with Sutherland-Hodgman.
    // Hint: visit every edge, including the edge from the last vertex to the first;
    // emit a crossing vertex when the endpoints are on opposite sides.
    /*************************************** TODO BEGIN ******************************************/
    if (count == 0) {
        return 0;
    }

    ClipVertex clipped[9];
    int clippedCount = 0;
    ClipVertex previous = polygon[count - 1];
    float previousSide = dot(plane, previous.clip);
    for (int i = 0; i < count; ++i) {
        ClipVertex current = polygon[i];
        float currentSide = dot(plane, current.clip);
        bool currentInside = currentSide >= 0.0;
        bool previousInside = previousSide >= 0.0;

        if (currentInside != previousInside) {
            float t = previousSide / (previousSide - currentSide);
            clipped[clippedCount].clip = mix(previous.clip, current.clip, t);
            clipped[clippedCount].worldPosition = mix(previous.worldPosition, current.worldPosition, t);
            clipped[clippedCount].normal = mix(previous.normal, current.normal, t);
            clipped[clippedCount].uv = mix(previous.uv, current.uv, t);
            ++clippedCount;
        }
        if (currentInside) {
            clipped[clippedCount] = current;
            ++clippedCount;
        }
        previous = current;
        previousSide = currentSide;
    }
    for (int i = 0; i < clippedCount; ++i) {
        polygon[i] = clipped[i];
    }
    return clippedCount;
    /**************************************** TODO END *******************************************/
}

void main() {
    ClipVertex polygon[9];
    int count = 3;
    for (int i = 0; i < 3; ++i) {
        polygon[i].clip = vertexData[i].clip;
        polygon[i].worldPosition = vertexData[i].worldPosition;
        polygon[i].normal = vertexData[i].normal;
        polygon[i].uv = vertexData[i].uv;
    }

    // OpenGL's homogeneous clip volume is -w <= x, y, z <= w.

    // Clip the source triangle against all six homogeneous clip planes.
    count = clipAgainst(polygon, count, vec4(1, 0, 0, 1));  // left: x + w >= 0
    count = clipAgainst(polygon, count, vec4(-1, 0, 0, 1)); // right: w - x >= 0
    count = clipAgainst(polygon, count, vec4(0, 1, 0, 1));  // bottom: y + w >= 0
    count = clipAgainst(polygon, count, vec4(0, -1, 0, 1)); // top: w - y >= 0
    count = clipAgainst(polygon, count, vec4(0, 0, 1, 1));  // near: z + w >= 0
    count = clipAgainst(polygon, count, vec4(0, 0, -1, 1)); // far: w - z >= 0

    // A convex clipped polygon becomes (0, 1, 2), (0, 2, 3), ... .
    for (int fan = 1; fan + 1 < count; ++fan) {
        ClipVertex triangle[3] = ClipVertex[3](polygon[0], polygon[fan], polygon[fan + 1]);
        ScreenVertex screenTriangle[3];
        // TODO: Convert each clip-space vertex to a ScreenVertex.
        // Hint: divide xyz by clip.w, map NDC xy to pixels and z to [0, 1],
        // and retain 1 / clip.w for perspective-correct interpolation.
        /*************************************** TODO BEGIN ******************************************/
        for (int i = 0; i < 3; ++i) {
            float reciprocalW = 1.0 / triangle[i].clip.w;
            vec3 ndc = triangle[i].clip.xyz * reciprocalW;
            screenTriangle[i].screenAndInvW = vec4((ndc.xy * 0.5 + 0.5) * uExtent,
                                                   ndc.z * 0.5 + 0.5, reciprocalW);
            screenTriangle[i].worldPosition = triangle[i].worldPosition;
            screenTriangle[i].normal = triangle[i].normal;
            screenTriangle[i].uv = triangle[i].uv;
        }

        vec2 a = screenTriangle[0].screenAndInvW.xy;
        vec2 b = screenTriangle[1].screenAndInvW.xy;
        vec2 c = screenTriangle[2].screenAndInvW.xy;
        // Reject back-facing and zero-area screen triangles.
        float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (area <= 0.0) {
            continue;
        }
        /**************************************** TODO END *******************************************/

        // TODO: Bound the candidate pixels and emit a primitive covering them.
        // Hint: Convert the screen-space bounds back to NDC; the fragment shader
        //       will perform the exact per-pixel triangle coverage test.
        // Hint: Use EmitVertex/EmitPrimitive
        /*************************************** TODO BEGIN ******************************************/
        vec2 minimum = min(a, min(b, c)) / uExtent * 2.0 - 1.0;
        vec2 maximum = max(a, max(b, c)) / uExtent * 2.0 - 1.0;
        // Pad the candidate region by one pixel so fixed-function edge rules
        // cannot omit a sample accepted by our fragment-shader edge test.
        vec2 padding = vec2(2.0) / uExtent;
        minimum = clamp(minimum - padding, vec2(-1.0), vec2(1.0));
        maximum = clamp(maximum + padding, vec2(-1.0), vec2(1.0));
        if (any(greaterThanEqual(minimum, maximum))) {
            continue;
        }

        // One right triangle covers the whole bounding rectangle. Three
        // vertices per fan triangle keep geometry-shader output below the
        // OpenGL 4.1 minimum total-output-components limit.
        vec2 corners[3] = vec2[3](minimum,
                                  vec2(2.0 * maximum.x - minimum.x, minimum.y),
                                  vec2(minimum.x, 2.0 * maximum.y - minimum.y));
        for (int corner = 0; corner < 3; ++corner) {
            for (int i = 0; i < 3; ++i) {
                gScreenAndInvW[i] = screenTriangle[i].screenAndInvW;
                gWorld[i] = screenTriangle[i].worldPosition;
                gNormal[i] = screenTriangle[i].normal;
                gUv[i] = screenTriangle[i].uv;
            }
            gl_Position = vec4(corners[corner], 0.0, 1.0);
            EmitVertex();
        }
        EndPrimitive();
        /**************************************** TODO END *******************************************/
    }
}
