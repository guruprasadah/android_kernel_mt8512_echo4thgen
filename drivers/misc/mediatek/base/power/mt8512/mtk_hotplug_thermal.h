enum cluster_type {CLUSTER_L, CLUSTER_B};

extern struct proc_dir_entry *mtk_thermal_get_proc_drv_therm_dir_entry(void);
extern void update_min_thermal_limit_cpu(unsigned int num);
extern void lock_power_throttle(void);
extern void unlock_power_throttle(void);
extern void update_thermal_limit_protect(void);

static int hpt_init(void);
int hpt_set_cpu_num_limit(unsigned int little_cpu,
		unsigned int big_cpu);
static void hpt_cpu_get_big_little_cpumasks(struct cpumask *big,
				     struct cpumask *little);
static int hpt_cpu_up(unsigned int cpu);
static int hpt_cpu_down(unsigned int cpu);

unsigned int num_online_little_cpus(void);

unsigned int num_online_big_cpus(void);
void get_cluster_cpu_id(unsigned int *cluster_cpu_id_min,
		unsigned int *cluster_cpu_id_max, enum cluster_type type);

unsigned int hpt_num_online_cpus(struct cpumask *cluster_cpu_mask,
		unsigned int **always_online, enum cluster_type type);
static void hotplug_thermal_limit(unsigned int cpu_num_limit,
		enum cluster_type type);

static void hpt_procfs_create(void);

extern int hpt_set_cpu_num_limit(
		unsigned int little_cpu, unsigned int big_cpu);
