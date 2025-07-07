local Camera = {}
Camera.__index = Camera

function Camera.new(a_pos, a_forward, a_right, a_up)
    local camera = 
    {
        pos = a_pos or float3(0),
        forward = a_forward or float3(0, 0, -1),
        right = a_right or float3(1, 0, 0),
        up = a_up or float3(0, 1, 0),
        yaw = 90,
        pitch = 0,
    }
    setmetatable(camera, Camera)
    return camera;
end

function Camera:Move(a_move)
    local move = float3(0)
    move = move + self.forward * a_move.z
    move = move + float3Normalize(float3Cross(self.forward, self.up)) * a_move.x
    move = move + self.up * a_move.y
    self.pos = self.pos + move
end

function Camera:Rotate(a_yaw, a_pitch)
    self.yaw = self.yaw + a_yaw
    self.pitch = self.pitch + a_pitch
    if self.pitch > 90.0 then
        self.pitch = 90.0
    elseif self.pitch < -90.0 then
        self.pitch = -90.0
    end
        
    local dir_x = math.cos(self.yaw) * math.cos(self.pitch)
    local dir_y = math.sin(self.pitch)
    local dir_z = math.sin(self.yaw) * math.cos(self.pitch)
    local direction = float3(dir_x, dir_y, dir_z)
        
    self.forward = float3Normalize(direction)
    local cross = float3Cross(self.forward, self.up)
    self.right = float3Normalize(cross)
end

local FreeCamera = {}
FreeCamera.__index = FreeCamera

function FreeCamera.new(a_cam, a_speed)
    local free_camera = 
    {
        camera = a_cam or Camera(),
        speed = a_speed or 1.0,
        min_speed = 1.0,
        max_speed = 100.0,
        velocity_speed = 25.0,
        velocity = float3(0)
    }
    setmetatable(free_camera, FreeCamera)
    return free_camera;
end

function FreeCamera:Move(a_move)
    self.velocity = self.velocity + a_move * self.speed
end

function FreeCamera:Rotate(a_yaw, a_pitch)
    self.camera:Rotate(a_yaw, a_pitch)
end
    
function FreeCamera:Update(a_delta_time)
    local velocity = self.velocity * self.velocity_speed * a_delta_time
    self.velocity = self.velocity - velocity
    self.camera:Move(velocity)
end

function FreeCamera:AddSpeed(a_speed)
    local speed = self.speed + a_speed * ((self.speed + 2.2) * 0.022)
    if speed > self.max_speed then
        speed = self.max_speed
    elseif speed < self.min_speed then
        speed = self.min_speed
    end
    self.speed = speed
end

function FreeCamera:LookAt(focus_pos)
    local right, up, forward = LookAt(focus_pos, self.camera.pos, self.camera.up)
    self.camera.forward = forward
    self.camera.right = right
    return right, up, forward
end

return {
    Camera = Camera,
    FreeCamera = FreeCamera
}
