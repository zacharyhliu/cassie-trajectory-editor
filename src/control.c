
#include "control.h"

#define SEL traj_info->selection

void load_pert(traj_info_t* traj_info)
{
    int body_id;
    int rootframe;
    double grabbed_node_transform[3];
    FILE* pfile = fopen("last.pert", "r");
    char buf[2048];
    char* result;
    ik_solver_params_t params;

    ik_default_fill_solver_params(&params);

    if(pfile)
    {
        result = fgets(buf,2048,pfile);
        if(!result)
            return;
        body_id = strtol(buf, NULL, 10);
        result = fgets(buf,2048,pfile);
        if(!result)
            return;
        rootframe = strtol(buf, NULL, 10);
        result = fgets(buf,2048,pfile);
        if(!result)
            return;
        SEL.nodesigma = strtod(buf, NULL);

        result = fgets(buf,2048,pfile);
        if(!result)
            return;
        grabbed_node_transform[0] = strtod(buf, NULL);
        result = fgets(buf,2048,pfile);
        if(!result)
            return;
        grabbed_node_transform[1] = strtod(buf, NULL);
        result = fgets(buf,2048,pfile);
        if(!result)
            return;
        grabbed_node_transform[2] = strtod(buf, NULL);

        fclose(pfile);

        node_perform_pert(
            traj_info,
            &params,
            grabbed_node_transform,
            node_get_cassie_id_from_index(body_id),
            rootframe
            );

        SEL.id_last_non_node_select = body_id;
    }
}

ik_solver_params_t* globparams = NULL;

void refine_pert(traj_info_t* traj_info)
{
    if(!globparams)
    {
        globparams = malloc(sizeof (ik_solver_params_t));
        ik_default_fill_solver_params(globparams);
    }
    
    globparams->ik_accuracy_cutoff /= 2;
    globparams->seedoption = IK_NEVER_SEED_LASTSOLN;
    globparams->pd_k_regular = 10000;
    globparams->pd_b_regular = 10;

    node_refine_pert(
        traj_info,
        globparams);
}

#define REVISUALIZE node_position_initial_using_cassie_body(traj_info, node_get_cassie_id_from_index(traj_info->selection.id_last_non_node_select))

void undo_pert(traj_info_t* traj_info)
{
    if(!traj_info->timeline->next)
        return;

    traj_info->timeline = traj_info->timeline->next;

    if(SEL.id_last_non_node_select > 0 && SEL.id_last_non_node_select <= 25)
        REVISUALIZE;
}

void redo_pert(traj_info_t* traj_info)
{
    if(!traj_info->timeline->prev)
        return;

    traj_info->timeline = traj_info->timeline->prev;

    if(SEL.id_last_non_node_select > 0 && SEL.id_last_non_node_select <= 25)
        REVISUALIZE;
}

void control_expand_pose(traj_info_t* traj_info)
{
    timeline_t* expanded;
    qpos_t* qpos;
    int frame;

    frame = timeline_get_frame_from_time(traj_info);
    qpos = timeline_get_qposes_from_frame(traj_info->timeline, frame);
    expanded = timeline_init_with_single_pose(qpos, traj_info->timeline);

    expanded->next = traj_info->timeline;
    traj_info->timeline->prev = expanded;
    traj_info->timeline = expanded;
}

// void transform_qpos(traj_info_t* traj_info, double sign)
// {
//     timeline_t* modified;
//     int i;

//     modified = timeline_duplicate(traj_info->timeline);
//     for(i = 0; i < modified->numposes; i++)
//     {
//         modified->qposes[i].q[traj_info->jointnum] += .05 * sign;
//     }

//     modified->next = traj_info->timeline;
//     traj_info->timeline->prev = modified;
//     traj_info->timeline = modified;
// }

void control_key_event(traj_info_t* traj_info, int key, int mods)
{
    if (*(traj_info->paused))
    {
        if(key == GLFW_KEY_RIGHT)
            traj_info->time_frozen -= 50000;
        if(key == GLFW_KEY_LEFT)
            traj_info->time_frozen += 50000;
        if(key == GLFW_KEY_DOWN)
            traj_info->time_frozen -= 500000;
        if (key == GLFW_KEY_UP)
            traj_info->time_frozen += 500000;
    }

    if( key==GLFW_KEY_PAGE_UP)
    {
        SEL.jointnum = (SEL.jointnum + 1) % CASSIE_QPOS_SIZE;
        if(SEL.node_type == NODE_JOINTID)
            REVISUALIZE;
    }
    else if( key==GLFW_KEY_PAGE_DOWN)
    {
        SEL.jointnum = (SEL.jointnum + CASSIE_QPOS_SIZE - 1) % CASSIE_QPOS_SIZE;
        if(SEL.node_type == NODE_JOINTID)
            REVISUALIZE;
    }
    else if (key == GLFW_KEY_MINUS)
    {
        NODECOUNT /= 1.5;
        REVISUALIZE;
    }
    else if (key == GLFW_KEY_EQUAL)
    {
        NODECOUNT *= 1.5;     
        REVISUALIZE;
    }
    else if(key == GLFW_KEY_ENTER)
    {
        SEL.node_type = (SEL.node_type + 1) % NODE_TYPE_E_COUNT;
        nodes_recolor(traj_info);
        REVISUALIZE;
    }

     if( mods & GLFW_MOD_CONTROL )
     {
        // if( key==GLFW_KEY_A )
        //     SEL.nodesigma *= .95;
        // else if( key==GLFW_KEY_D)
        //     SEL.nodesigma *= 1.05;
        // else if( key==GLFW_KEY_S )
        //     SEL.nodeheight *= .95;
        // else if( key==GLFW_KEY_W)
        //     SEL.nodeheight *= 1.05;
        if( key==GLFW_KEY_P)
            load_pert(traj_info);
        else if( key==GLFW_KEY_R)
            refine_pert(traj_info);
        else if( key==GLFW_KEY_E)
            control_expand_pose(traj_info);
        else if( key==GLFW_KEY_Z &&  !(mods & GLFW_MOD_SHIFT) )
            undo_pert(traj_info);
        else if( key==GLFW_KEY_Y || (
            key==GLFW_KEY_Z &&  (mods & GLFW_MOD_SHIFT)  ))
            redo_pert(traj_info);
    }

}



