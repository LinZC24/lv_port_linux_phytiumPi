#include "top_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define CHART_POINT_COUNT 20

/*********************
 *      TYPEDEFS
 *********************/
typedef struct {
    lv_obj_t * arc;       /* 替换 meter 为 arc */
    lv_obj_t * label_val;
    lv_obj_t * label_info;
    lv_obj_t * chart;
    lv_chart_series_t * ser;
    lv_obj_t * win;
    const char * title;
    int (*get_value_cb)(void);
    int last_value;
} monitor_item_t;

/*********************
 *  STATIC VARIABLES
 *********************/
 static long mem_total_kb = 0;
static long mem_used_kb = 0;
static monitor_item_t cpu_mon;
static monitor_item_t gpu_mon;
static monitor_item_t mem_mon;
static lv_timer_t * monitor_timer;

/*********************
 *  HELPER FUNCTIONS
 *********************/

/* 读取 CPU 使用率 (读取 /proc/stat) */
static int get_cpu_usage(void)
{
    static unsigned long long prev_total = 0, prev_idle = 0;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    FILE *fp = fopen("/proc/stat", "r");
    if(!fp) return 0;

    if(fscanf(fp, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
              &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    unsigned long long idle_time = idle + iowait;
    unsigned long long total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long total_diff = total_time - prev_total;
    unsigned long long idle_diff = idle_time - prev_idle;

    prev_total = total_time;
    prev_idle = idle_time;

    if(total_diff == 0) return 0;
    return (int)((total_diff - idle_diff) * 100 / total_diff);
}

/* 读取内存使用率 (读取 /proc/meminfo) */
static int get_mem_usage(void)
{
    long mem_available = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if(!fp) return 0;

    char key[32];
    long value;
    char unit[8];
    
    // 重置读取
    mem_total_kb = 0;
    
    while(fscanf(fp, "%31s %ld %7s", key, &value, unit) == 3) {
        if(strcmp(key, "MemTotal:") == 0) mem_total_kb = value;
        else if(strcmp(key, "MemAvailable:") == 0) { mem_available = value; break; }
    }
    fclose(fp);

    if(mem_total_kb == 0) return 0;
    
    // 计算已用内存
    mem_used_kb = mem_total_kb - mem_available;
    
    return (int)(mem_used_kb * 100 / mem_total_kb);
}

/*********************
 *  UI FUNCTIONS
 *********************/

static void close_win_cb(lv_event_t * e)
{
    lv_obj_t * win = lv_event_get_user_data(e);
    lv_obj_add_flag(win, LV_OBJ_FLAG_HIDDEN);
}

static void meter_click_cb(lv_event_t * e)
{
    monitor_item_t * item = (monitor_item_t *)lv_event_get_user_data(e);
    if(item->win) {
        lv_obj_remove_flag(item->win, LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_monitor_widget(lv_obj_t * parent, monitor_item_t * item, const char * title, int (*cb)(void))
{
    item->title = title;
    item->get_value_cb = cb;
    item->last_value = 0;

    /* --- 1. 仪表盘部分 (保持不变) --- */
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 240, 240);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(cont, meter_click_cb, LV_EVENT_CLICKED, item);

    lv_obj_t * label = lv_label_create(cont);
    lv_label_set_text(label, title);

    item->arc = lv_arc_create(cont);
    lv_obj_set_size(item->arc, 160, 160);
    lv_arc_set_rotation(item->arc, 135);
    lv_arc_set_bg_angles(item->arc, 0, 270);
    lv_arc_set_value(item->arc, 0);
    lv_obj_remove_style(item->arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(item->arc, LV_OBJ_FLAG_CLICKABLE);

    item->label_info = lv_label_create(cont);
    lv_label_set_text(item->label_info, ""); // 默认空，只有内存会更新它
    lv_obj_set_style_text_font(item->label_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(item->label_info, lv_palette_main(LV_PALETTE_GREY), 0);

    item->label_val = lv_label_create(cont);
    lv_label_set_text(item->label_val, "0%");

    /* --- 2. 弹窗与图表部分 (使用 Grid 布局修复错位) --- */
    item->win = lv_win_create(lv_screen_active());
    lv_win_add_title(item->win, title);
    
    lv_obj_t * btn = lv_win_add_button(item->win, LV_SYMBOL_CLOSE, 60);
    lv_obj_add_event_cb(btn, close_win_cb, LV_EVENT_CLICKED, item->win);
    
    lv_obj_add_flag(item->win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(item->win, 600, 400);
    lv_obj_center(item->win);

    /* 获取窗口内容容器 */
    lv_obj_t * win_content = lv_win_get_content(item->win);
    lv_obj_set_layout(win_content, LV_LAYOUT_GRID);
    
    /* --- 修改 1: 调整行高定义 --- */
    /* 
       原: {LV_GRID_FR(1), 40, 20, ...} 
       问题: FR(1) 会把剩余空间全吃掉，导致图表区域很大，如果图表没填满就会有空隙。
       修改: 将图表行设为 FR(1) 依然没问题，关键是让 X 轴紧贴上去。
       我们把 X 轴行高设为 LV_GRID_CONTENT，让它紧凑一点。
    */
    static int32_t col_dsc[] = {40, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; // Y轴宽度减小到40
    static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(win_content, col_dsc, row_dsc);

    /* --- Y 轴刻度 --- */
    lv_obj_t * scale_y = lv_scale_create(win_content);
    lv_obj_set_grid_cell(scale_y, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_scale_set_mode(scale_y, LV_SCALE_MODE_VERTICAL_LEFT);
    lv_scale_set_range(scale_y, 0, 100);
    lv_scale_set_total_tick_count(scale_y, 11);
    lv_scale_set_major_tick_every(scale_y, 5);
    lv_obj_set_style_line_color(scale_y, lv_palette_main(LV_PALETTE_GREY), 0);

    /* --- 图表 --- */
    item->chart = lv_chart_create(win_content);
    lv_obj_set_grid_cell(item->chart, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_chart_set_type(item->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(item->chart, CHART_POINT_COUNT);
    lv_chart_set_range(item->chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_border_width(item->chart, 1, 0);
    lv_obj_set_style_border_color(item->chart, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    /* 关键: 移除图表底部的内边距，让它能紧贴 X 轴 */
    lv_obj_set_style_pad_bottom(item->chart, 0, 0);

    /* --- X 轴刻度 --- */
    lv_obj_t * scale_x = lv_scale_create(win_content);
    lv_obj_set_grid_cell(scale_x, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 1, 1);
    
    /* --- 修改 2: 调整 X 轴高度和模式 --- */
    lv_obj_set_height(scale_x, 40); // 显式设置高度
    
    /* 
       LV_SCALE_MODE_HORIZONTAL_TOP: 刻度线在尺子上方 (指向图表)
       LV_SCALE_MODE_HORIZONTAL_BOTTOM: 刻度线在尺子下方 (指向文字)
       
       如果你觉得现在的刻度线在数字上方很怪，可以改回 BOTTOM。
       但为了紧贴图表，TOP 是对的，只是我们需要把数字移到下面去。
       
       简单方案：改回 BOTTOM，并让它向上偏移。
    */
    lv_scale_set_mode(scale_x, LV_SCALE_MODE_HORIZONTAL_TOP); 
    
    /* 关键: 强制让 X 轴向上移动，消除间隙 */
    lv_obj_set_style_margin_top(scale_x, -10, 0); // 负边距拉近距离

    lv_scale_set_range(scale_x, 0, 20);
    lv_scale_set_total_tick_count(scale_x, 11);
    lv_scale_set_major_tick_every(scale_x, 5);
    lv_obj_set_style_line_color(scale_x, lv_palette_main(LV_PALETTE_GREY), 0);

    /* --- X 轴标题 --- */
    lv_obj_t * x_label = lv_label_create(win_content);
    lv_label_set_text(x_label, "Time (seconds)");
    lv_obj_set_grid_cell(x_label, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    /* 稍微向上一点 */
    lv_obj_set_style_margin_top(x_label, -5, 0);

    /* 添加数据系列 */
    item->ser = lv_chart_add_series(item->chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
}

static void update_timer_cb(lv_timer_t * timer)
{
    (void)timer;
    monitor_item_t * items[] = {&cpu_mon, &gpu_mon, &mem_mon};

    for(int i=0; i<3; i++) {
        monitor_item_t * item = items[i];
        if(!item->get_value_cb) continue;

        int val = item->get_value_cb();
        item->last_value = val;

        /* 更新 Arc */
        lv_arc_set_value(item->arc, val);
        lv_label_set_text_fmt(item->label_val, "%d%%", val);

        /* [新增] 如果是内存监视器，更新具体数值标签 */
        if(item == &mem_mon) {
            // 将 KB 转换为 MB 显示
            int used_mb = mem_used_kb / 1024;
            int total_mb = mem_total_kb / 1024;
            lv_label_set_text_fmt(item->label_info, "%dMB / %dMB", used_mb, total_mb);
        }

        /* 更新折线图 */
        if(item->chart) {
            lv_chart_set_next_value(item->chart, item->ser, val);
        }
    }
}

void top_demo_init(void)
{
    lv_obj_t * scr = lv_screen_active();
    
    /* 创建主布局容器 */
    lv_obj_t * main_cont = lv_obj_create(scr);
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0xF0F0F0), 0);

    /* 创建三个监视器 */
    create_monitor_widget(main_cont, &cpu_mon, "CPU Usage(%)", get_cpu_usage);

    create_monitor_widget(main_cont, &mem_mon, "Memory Usage(%)", get_mem_usage);

    /* 启动定时器 */
    monitor_timer = lv_timer_create(update_timer_cb, 1000, NULL);
}