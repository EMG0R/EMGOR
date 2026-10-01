from kivy.app import App
from kivy.uix.widget import Widget
from kivy.graphics import Color, Ellipse, Rectangle
from kivy.clock import Clock
import random
import math
from kivy.core.window import Window

# Set window size (optional, not needed in full-screen mode)
Window.size = (470, 800)

class CircleGroup:
    """A class representing a group of concentric circles moving as a unit."""
    def __init__(self, x, y, velocity_x, velocity_y, brightness_factor, base_radius, size_change_rate, background_color, speed):
        self.x = x
        self.y = y
        self.velocity_x = velocity_x
        self.velocity_y = velocity_y
        self.speed = speed  # Speed is now unique per group
        self.brightness_factor = brightness_factor
        self.base_radius = base_radius  # Larger variation in size
        self.current_radius_x = self.base_radius  # Two radii for shape change
        self.current_radius_y = self.base_radius
        self.size_change_rate = size_change_rate
        self.size_direction = 1  # Controls whether the group is growing or shrinking
        self.brightness_target = brightness_factor  # Target brightness for smooth transition
        self.brightness_transition_speed = random.uniform(0.0005, 0.001)  # Brightness transition speed
        self.current_color = self.random_bright_color(background_color)  # Groups now have slightly random bright colors
        self.target_color = self.random_color_close_to_bg(background_color)  # More variation but close to background color

    def random_color_close_to_bg(self, bg_color):
        """Generate a random color close to the given background color."""
        return [min(1, max(0, bg_color[i] + random.uniform(-0.05, 0.05))) for i in range(3)] + [1]

    def random_bright_color(self, base_color):
        """Generate a brighter, slightly random color for each group."""
        return [min(1, base_color[i] + random.uniform(0.2, 0.4)) for i in range(3)] + [1]  # Brighter color with full alpha

class LavaLampEffect(Widget):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.color_transition_speed = 0.001  # Background color change speed
        self.group_color_transition_speed = 0.0003  # Group color change speed
        self.current_color = self.random_color()
        self.target_color = self.random_color()
        
        # Background setup
        with self.canvas:
            self.bg_color = Color(*self.current_color)
            self.rect = Rectangle(pos=self.pos, size=self.size)
        
        self.bind(pos=self.update_rect, size=self.update_rect)

        # Create groups of circles
        self.groups = []
        Clock.schedule_once(self.create_circle_groups, 0)
        
        # Schedule updates
        Clock.schedule_interval(self.update_background, 1 / 60)
        Clock.schedule_interval(self.update_groups, 1 / 60)

    def create_circle_groups(self, dt):
        """Create random groups of circles starting at random positions."""
        for _ in range(10):  # 10 groups with more variation
            center_x = random.uniform(0, self.width)  # Random starting position
            center_y = random.uniform(0, self.height)
            speed = random.uniform(100, 300)  # Faster speed for each group
            angle = random.uniform(0, 2 * math.pi)  # Random direction
            brightness_factor = random.uniform(1.2, 1.5)  # Brighter overall groups
            base_radius = random.randint(50, 180)  # Larger variation in base radius size
            size_change_rate = random.uniform(0.1, 0.3)  # Slower size changes
            velocity_x = math.cos(angle) * speed
            velocity_y = math.sin(angle) * speed
            
            self.groups.append(CircleGroup(center_x, center_y, velocity_x, velocity_y, brightness_factor, base_radius, size_change_rate, self.current_color, speed))

    def update_rect(self, *args):
        self.rect.pos = self.pos
        self.rect.size = self.size

    def random_color(self):
        """Generate a random color."""
        return [random.uniform(0.3, 0.6) for _ in range(3)] + [1]

    def update_background(self, dt):
        """Smooth background color transition."""
        for i in range(3):
            if abs(self.target_color[i] - self.current_color[i]) > 0.01:
                self.current_color[i] += self.color_transition_speed * (
                    1 if self.target_color[i] > self.current_color[i] else -1
                )
        self.bg_color.rgba = self.current_color

        if all(abs(self.target_color[i] - self.current_color[i]) < 0.01 for i in range(3)):
            self.target_color = self.random_color()

    def update_groups(self, dt):
        num_layers = 20  # 20 circles per group
        self.canvas.after.clear()

        # Sort groups by brightness
        self.groups.sort(key=lambda g: g.brightness_factor, reverse=True)

        for i, group in enumerate(self.groups):
            # Move the group at its own speed
            group.x += group.velocity_x * dt
            group.y += group.velocity_y * dt

            # Get outer radius
            max_radius_x = group.current_radius_x * (1 + (num_layers - 1) * 0.1)
            max_radius_y = group.current_radius_y * (1 + (num_layers - 1) * 0.1)

            # Always bounce off the walls (no bouncing off each other)
            if group.x - max_radius_x < 0 or group.x + max_radius_x > self.width:
                group.velocity_x *= -1  # Reverse direction on collision with wall
            if group.y - max_radius_y < 0 or group.y + max_radius_y > self.height:
                group.velocity_y *= -1  # Reverse direction on collision with wall

            # Drastic shape changes (circles to ellipses to lines)
            group.current_radius_x += group.size_change_rate * group.size_direction * random.uniform(0.8, 1.5)
            group.current_radius_y += group.size_change_rate * group.size_direction * random.uniform(0.4, 2.0)  # More variety in shape changes
            if group.current_radius_x >= group.base_radius * 2.5:  # Allow for larger deformations
                group.size_direction = -1  # Shrink
            elif group.current_radius_x <= group.base_radius * 0.5:
                group.size_direction = 1  # Grow

            # Brightness transitions
            if abs(group.brightness_target - group.brightness_factor) > 0.01:
                group.brightness_factor += group.brightness_transition_speed * (
                    1 if group.brightness_target > group.brightness_factor else -1
                )
            else:
                group.brightness_target = random.uniform(1.2, 1.5)  # Slightly higher brightness

            # Draw circles/ellipses/lines within each group
            for j in range(num_layers):
                brightness = group.brightness_factor - (j / (num_layers - 1) * (group.brightness_factor - 1))
                
                layer_color = [min(1, group.current_color[k] * brightness) for k in range(3)] + [0.8 - j * (0.4 / num_layers)]  # More solid, brighter colors
                layer_radius_x = group.current_radius_x * (1 + j * 0.1)
                layer_radius_y = group.current_radius_y * (1 + j * 0.1)
                
                with self.canvas.after:
                    Color(*layer_color)
                    Ellipse(
                        pos=(group.x - layer_radius_x, group.y - layer_radius_y),
                        size=(layer_radius_x * 2, layer_radius_y * 2)
                    )

class LavaLampApp(App):
    def build(self):
        # Set the application to full-screen mode
        Window.fullscreen = True
        
        # Hide the mouse cursor
        Window.show_cursor = False
        
        return LavaLampEffect()

if __name__ == "__main__":
    try:
        LavaLampApp().run()
    except KeyboardInterrupt:
        print("Program interrupted.")
