
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <asm/pgtable.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/pid_namespace.h>
#include <asm/io.h>


MODULE_LICENSE("GPL");

unsigned long vrt2phys(struct mm_struct * mm, unsigned long vpage);

void proc_count(void);
void log_write(void);
int show_report(struct seq_file *m, void *v);
int open_report(struct inode *inode, struct  file *file);
int proc_init (void);
void proc_cleanup(void);

typedef struct counter {
  
  unsigned long pid;
  char * name;
  
  unsigned long contiguous;
  unsigned long noncontiguous;
  unsigned long total_pages;
  
  struct counter * next;
} list_counter;

const struct proc_ops proc_ops = {
  .proc_lseek = seq_lseek,
  .proc_release = single_release,
  .proc_open = open_report,
  .proc_read = seq_read,

};

unsigned long total_contiguous = 0;
unsigned long total_noncontiguous = 0;
unsigned long page_total = 0;

list_counter *linked_head = NULL;

int proc_init (void) { 
  
  printk(KERN_INFO "helloModule: kernel module initialized\n");
  
  proc_count();
  log_write();
  
  proc_create("Report", 0, NULL, &proc_ops);

  return 0;
}

void proc_count(void) {

  struct task_struct *task;
  struct vm_area_struct *vma;
  
  list_counter *curr = linked_head;
  list_counter *node;
  
  unsigned long vpage;
  unsigned long previous_address;
  unsigned long physical = 0;

  for_each_process(task) {

    if (task->pid > 650) { 
    
      node = kmalloc(sizeof(list_counter), GFP_KERNEL);
      
      node->pid = task->pid;
      node->name = task->comm;
      
      node->contiguous = 0;
      node->noncontiguous = 0;
      node->total_pages = 0;
      
      node->next = NULL;

      vma = 0;
      previous_address = 0;
      
      if (task->mm && task->mm->mmap) {      
      for (vma = task->mm->mmap; vma; vma = vma->vm_next) {
        previous_address = 0;
      for (vpage = vma->vm_start; vpage < vma->vm_end; vpage += PAGE_SIZE) {          
        physical = vrt2phys(task->mm, vpage);            
      if (physical != 0) {            
        node->total_pages += 1;             
      if ((previous_address + PAGE_SIZE) - physical == 0) {              
        node->contiguous += 1;                
      } else {              
        node->noncontiguous += 1;                
      }              
        previous_address = physical;
      	  }
        }
      }

        total_contiguous += node->contiguous;
        total_noncontiguous += node->noncontiguous;
        page_total += node->total_pages;
      }

      if(linked_head == NULL) {
        linked_head = node;
        curr = linked_head;    
      } else {
        curr->next = node;
        curr = curr->next;
      }
    }
  }
}

unsigned long vrt2phys(struct mm_struct * mm, unsigned long vpage) { 
  pgd_t *pgd;
  p4d_t *p4d;
  pud_t *pud;
  pmd_t *pmt;
  pte_t *pte;
  struct page *page;
  pgd = pgd_offset(mm, vpage);
  if (pgd_none(*pgd) || pgd_bad(*pgd))
    return 0;
  p4d = p4d_offset(pgd, vpage);
  if (p4d_none(*p4d) || p4d_bad(*p4d))
    return 0;
  pud = pud_offset(p4d, vpage);
  if (pud_none(*pud) || pud_bad(*pud))
    return 0;
   pmt = pmd_offset(pud, vpage);
  if (pmd_none(*pmt) || pmd_bad(*pmt))
    return 0;
  if (!(pte = pte_offset_map(pmt, vpage)))
    return 0;
  if (!(page = pte_page(*pte)))
    return 0;
  unsigned long physical_page_addr = page_to_phys(page);
  pte_unmap(pte);
  if (physical_page_addr==70368744173568)
  	return 0;
  return physical_page_addr;
}

void log_write(void) {

  list_counter * this_node;
  this_node = linked_head;
  
  while (this_node) {
    
    printk("%lu,%s,%lu,%lu,%lu", this_node->pid, this_node->name,
        this_node->contiguous, this_node->noncontiguous, this_node->total_pages);
    this_node = this_node->next;
  }
  
  printk("TOTALS,,%lu,%lu,%lu",
    total_contiguous, total_noncontiguous, page_total); 
}

void proc_cleanup(void) {

  printk(KERN_INFO "helloModule: performing cleanup of module\n");
  remove_proc_entry("Report", NULL);
  
}

int show_report(struct seq_file *m, void *v) {

  list_counter *item = linked_head;
  list_counter *previous = item;
  
  seq_printf(m, "PROCESS REPORT: \nproc_id,proc_name,contig_pages,noncontig_pages,total_pages \n");

  while (item) {
    
    seq_printf(m, "%lu,%s,%lu,%lu,%lu\n", item->pid, item->name,
        item->contiguous, item->noncontiguous, item->total_pages);
        
    previous = item;
    item = item->next;
    
    kfree(previous);
  }
  
  seq_printf(m, "TOTALS,,%lu,%lu,%lu\n",
    total_contiguous, total_noncontiguous, page_total); 

  return 0;
}

int open_report(struct inode *inode, struct  file *file) {
  return single_open(file, show_report, NULL);
}

module_init(proc_init);
module_exit(proc_cleanup);
