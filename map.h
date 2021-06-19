#define MAP_SIZE 4
struct line
{
    int x1;
    int x2;
    int y1;
    int y2;
};

struct line map[] = {
    {-150, -150, 150, -150},
    {150, -150, 150, 150},
    {150, 150, -150, 150},
    {-150, 150, -150, -150}};