function LookAt(a_center, a_eye, a_up)
    local new_forward = float3Normalize(a_center - a_eye)
    local new_right = float3Normalize(float3Cross(new_forward, a_up))
    local new_up = float3Cross(new_right, new_forward)

    return new_right, new_up, new_forward
end
